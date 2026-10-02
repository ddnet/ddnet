use std::future;
use std::sync::LazyLock;
use std::thread;
use tokio::runtime;

/// Gets a handle to the runtime that you can use to spawn `async` tasks.
///
/// On the first call, the runtime is initialized.
///
/// The tasks are simply aborted when the process exits, we don't wait for them
/// to finish.
///
/// # Example
///
/// ```
/// # extern crate ddnet_test;
/// use ddnet_engine_shared::runtime;
/// use std::time::Duration;
/// use tokio::sync::oneshot;
/// use tokio::time::sleep;
///
/// async fn lengthy_computation() -> i32 {
///    sleep(Duration::from_millis(100)).await;
///    3
/// }
///
/// let (tx, rx) = oneshot::channel();
///
/// runtime().spawn(async move {
///     tx.send(lengthy_computation().await).unwrap();
/// });
///
/// assert_eq!(rx.blocking_recv().unwrap(), 3)
/// ```
pub fn runtime() -> &'static runtime::Handle {
    static RUNTIME: LazyLock<runtime::Handle> = LazyLock::new(|| {
        let runtime = runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .expect("couldn't build runtime");

        let handle = runtime.handle().clone();

        // Spawn worker thread so that the runtime has something to work with.
        thread::spawn(move || runtime.block_on(future::pending::<()>()));

        handle
    });
    &RUNTIME
}
