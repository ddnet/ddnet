#[tokio::main(flavor = "current_thread")]
async fn main() -> anyhow::Result<()> {
    asdf::bind().await
}
