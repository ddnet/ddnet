use anyhow::Context as _;
use asdf::Asdf;
use clap::App;
use clap::Arg;
use clap::value_t_or_exit;
use std::future::pending;
use std::net::SocketAddr;

#[tokio::main(flavor = "current_thread")]
async fn main() -> anyhow::Result<()> {
    env_logger::init();

    let command = App::new("asdf")
        .about("Provide coordination for DDNet game servers")
        .arg(Arg::with_name("listen")
            .long("listen")
            .value_name("ADDRESS")
            .help("Listen address.")
        )
        .arg(Arg::with_name("upstream")
            .long("upstream")
            .value_name("ADDRESS")
            .help("Connect to this upstream asdf server.")
        );

    let matches = command.get_matches();

    let listen_address = matches.is_present("listen").then(|| {
        value_t_or_exit!(matches.value_of("listen"), SocketAddr)
    });
    let upstream_address = matches.is_present("upstream").then(|| {
        value_t_or_exit!(matches.value_of("upstream"), SocketAddr)
    });

    match (listen_address, upstream_address) {
        (Some(listen_address), None) => {
            Asdf::listen(listen_address).context("asdf")?;
            pending().await
        }
        _ => todo!(),
    }
}
