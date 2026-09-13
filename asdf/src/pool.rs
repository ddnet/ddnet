use futures_util::FutureExt as _;
use futures_util::Stream;
use futures_util::StreamExt as _;
use futures_util::stream::FuturesUnordered;
use std::future::Future;
use std::pin::Pin;
use std::task::Context;
use std::task::Poll;
use tokio::sync::mpsc;
use tokio::task::JoinError;
use tokio::task::JoinHandle;

struct Task {
    name: &'static str,
    handle: JoinHandle<anyhow::Result<()>>,
}

#[non_exhaustive]
pub struct TaskResult {
    pub name: &'static str,
    pub result: Result<anyhow::Result<()>, JoinError>,
}

/// A collection of tasks that can be cancelled.
///
/// When it is dropped, all tasks in it are cancelled.
pub struct TaskPool {
    tasks: FuturesUnordered<Task>,
    incoming: mpsc::Receiver<Task>,
}

/// A handle to a [`TaskPool`] that can be used to spawn new tasks.
#[derive(Clone)]
pub struct TaskPoolHandle {
    task_pool: mpsc::Sender<Task>,
}

impl Future for Task {
    type Output = TaskResult;
    fn poll(mut self: Pin<&mut Self>, cx: &mut Context) -> Poll<TaskResult> {
        self.handle.poll_unpin(cx).map(|result| TaskResult {
            name: self.name,
            result,
        })
    }
}

impl TaskPool {
    pub fn new() -> (TaskPool, TaskPoolHandle) {
        let (outgoing, incoming) = mpsc::channel(1);
        let task_pool = TaskPool {
            tasks: Default::default(),
            incoming,
        };
        let handle = TaskPoolHandle {
            task_pool: outgoing,
        };
        (task_pool, handle)
    }
    pub async fn close(mut self) {
        // Make sure all successfully sent tasks actually arrive.
        self.incoming.close();
        while let Some(task) = self.incoming.recv().await {
            self.tasks.push(task)
        }
        // Drop self, abort all tasks.
    }
}

impl Drop for TaskPool {
    fn drop(&mut self) {
        for task in &self.tasks {
            task.handle.abort();
        }
    }
}

/// This is a stream of the finished tasks of the task pool.
impl Stream for TaskPool {
    type Item = TaskResult;
    fn poll_next(mut self: Pin<&mut Self>, cx: &mut Context) -> Poll<Option<TaskResult>> {
        loop {
            match self.incoming.poll_recv(cx) {
                Poll::Pending => break, // no new items
                Poll::Ready(None) => break, // no new items and all senders dropped
                Poll::Ready(Some(task)) => self.tasks.push(task),
            }
        }
        self.tasks.poll_next_unpin(cx)
    }
    fn size_hint(&self) -> (usize, Option<usize>) {
        self.tasks.size_hint()
    }
}

impl TaskPoolHandle {
    pub async fn spawn<F>(&self, name: &'static str, future: F) where
        F: Future<Output=anyhow::Result<()>> + Send + 'static
    {
        match self.task_pool.reserve().await {
            Ok(permit) => permit.send(Task {
                handle: tokio::spawn(future),
                name,
            }),
            Err(mpsc::error::SendError(())) => {
                // Don't spawn the new future, silently drop it, because the
                // task pool it belongs to has already been closed.
            }
        }
    }
}

/// Creates a task pool with a main task and error handling.
///
/// The whole task pool quits when the main task quits.
///
/// When any task in the pool errors or panics, the `error` function is called.
///
/// # Errors
///
/// Returns an error (in addition to calling the `error` function) when any
/// tasks errors or panics.
pub async fn task_pool_with_main_and_error<M, E, MF, EF>(main: M, error: E)
    -> anyhow::Result<()>
where
    M: FnOnce(TaskPoolHandle) -> MF,
    E: FnOnce(protocol::CloseMessage) -> EF,
    MF: Future<Output = anyhow::Result<()>> + Send + 'static,
    EF: Future<Output = anyhow::Result<()>> + Send + 'static,
{
    let (mut task_pool, handle) = TaskPool::new();
    handle.clone().spawn("main", main(handle)).await;
    while let Some(task) = task_pool.next().await {
        let result: Result<anyhow::Result<Infallible>, JoinError> = match task.result {
            Ok(Ok(())) => {
                if task.name == "main" {
                    // The main task quit, let's quit as well.
                    task_pool.close().await;
                    return Ok(());
                } else {
                    // Another task quit, nothing to do.
                    continue;
                }
            }
            Ok(Err(err)) => Ok(Err(err)),
            Err(err) => Err(err),
        };
        let close_message = error_close(task.name, &result);
        let error_result = error(close_message).await;
        task_pool.close().await;
        let () = error_result
            .with_context(|| format!("sending error failed while processing another error: {}", error_close(task.name, &result).error.unwrap()))?;
        let infallible = result
            .with_context(|| format!("task {} errored", task.name))?
            .with_context(|| format!("task {} panicked", task.name))?;
        match infallible {}
    }
    // main task has to quit or error before this.
    unreachable!();
}
