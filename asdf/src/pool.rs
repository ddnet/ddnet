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

pub struct TaskPool {
    tasks: FuturesUnordered<Task>,
    incoming: mpsc::Receiver<Task>,
}

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
