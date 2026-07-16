use asdf::Backend;
use asdf::State;
use std::pin::Pin;
use tokio::io::AsyncBufReadExt as _;
use tokio::io::AsyncRead;
use tokio::io::AsyncWrite;
use tokio::io::AsyncWriteExt as _;
use tokio::io::BufReader;
use tokio_util::io::simplex as tokio_util_simplex;

fn simplex(capacity: usize) -> (tokio_util_simplex::Receiver, tokio_util_simplex::Sender) {
    let (tx, rx) = tokio_util_simplex::new(capacity);
    (rx, tx)
}

struct Connection {
    rx: BufReader<Pin<Box<dyn AsyncRead + Send + Sync>>>,
    tx: Pin<Box<dyn AsyncWrite + Send + Sync>>,
    buf: String,
}

impl Connection {
    fn new(rx: Pin<Box<dyn AsyncRead + Send + Sync>>, tx: Pin<Box<dyn AsyncWrite + Send + Sync>>) -> Connection {
        Connection {
            rx: BufReader::new(rx),
            tx,
            buf: String::new(),
        }
    }
    async fn read_line<'a>(&'a mut self) -> Option<&'a str> {
        self.buf.clear();
        self.rx.read_line(&mut self.buf).await.unwrap();
        match self.buf.as_bytes().last() {
            None => {
                println!("< EOF");
                return None;
            }
            Some(b'\n') => {}
            Some(_) => panic!("read incomplete line"),
        }
        let line = &self.buf[..self.buf.len() - 1];
        println!("< {line}");
        Some(line)
    }
    async fn write_line(&mut self, line: &str) {
        println!("> {line}");
        self.tx.write_all(line.as_bytes()).await.unwrap();
        self.tx.write_all(b"\n").await.unwrap();
    }
    async fn close_write(&mut self) {
        println!("> EOF");
        self.tx.shutdown().await.unwrap()
    }
    async fn conversation(&mut self, conversation: &str) {
        for line in conversation.lines() {
            if line.is_empty() {
                continue;
            } else if line == "> EOF" {
                self.close_write().await;
            } else if line.starts_with("> ") {
                self.write_line(&line[2..]).await;
            } else if line == "< EOF" {
                assert_eq!(self.read_line().await, None);
            } else if line.starts_with("< ") {
                assert_eq!(self.read_line().await, Some(&line[2..]));
            } else if line.starts_with("# ") {
                // comment
                continue;
            } else {
                panic!("invalid line: {line}");
            }
        }
    }
}

fn one_client() -> Connection {
    let state = State::default();
    let (rx_server, tx) = simplex(4096);
    let (rx, tx_server) = simplex(4096);
    let connection = Connection::new(Box::pin(rx), Box::pin(tx));

    let _ = tokio::spawn(async move {
        asdf::handle_client_connection(Box::pin(rx_server), Box::pin(tx_server), state.clone(), Backend::Local(state)).await.unwrap();
    });
    connection
}

fn server() -> Connection {
    let state = State::default();
    let (rx, tx_client) = simplex(4096);
    let (rx_client, tx) = simplex(4096);
    let connection = Connection::new(Box::pin(rx), Box::pin(tx));

    let _ = tokio::spawn(async move {
        asdf::handle_server_connection(Box::pin(rx_client), Box::pin(tx_client), state).await.unwrap();
    });
    connection
}

#[tokio::test]
async fn test_server_smoke() {
    one_client().conversation(r#"
> {"kind":"client_hello","protocol_version_min":1,"protocol_version_max":1}
< {"kind":"server_hello","protocol_version":1}
> {"kind":"add_ban","net":"127.0.0.1/32","expiry":12345,"reason":"foobar"}
> {"kind":"subscribe_bans"}
< {"kind":"replace_bans","bans":[{"net":"127.0.0.1/32","expiry":12345,"reason":"foobar"}]}
> {"kind":"add_ban","net":"127.0.0.1/32","expiry":12345,"reason":"foobar"}
> {"kind":"add_ban","net":"127.0.0.2/32","expiry":54321,"reason":"barfoo"}
< {"kind":"replace_bans","bans":[{"net":"127.0.0.1/32","expiry":12345,"reason":"foobar"},{"net":"127.0.0.2/32","expiry":54321,"reason":"barfoo"}]}
> {"kind":"close"}
> EOF
< {"kind":"close"}
< EOF
"#).await;
}

#[tokio::test]
async fn test_server_notjson() {
    one_client().conversation(r#"
> not json
< {"kind":"close","error":"invalid message read: expected ident at line 1 column 2"}
< EOF
"#).await;
}

#[tokio::test]
async fn test_server_version_0_fails() {
    one_client().conversation(r#"
> {"kind":"client_hello","protocol_version_min":0,"protocol_version_max":0}
< {"kind":"close","error":"incompatible client version request"}
< EOF
"#).await;
}

#[tokio::test]
async fn test_server_version_1_works() {
    one_client().conversation(r#"
> {"kind":"client_hello","protocol_version_min":1,"protocol_version_max":1}
< {"kind":"server_hello","protocol_version":1}
"#).await;
}

#[tokio::test]
async fn test_server_version_selection_works_below() {
    one_client().conversation(r#"
> {"kind":"client_hello","protocol_version_min":0,"protocol_version_max":1}
< {"kind":"server_hello","protocol_version":1}
"#).await;
}

#[tokio::test]
async fn test_server_version_selection_works_above() {
    one_client().conversation(r#"
> {"kind":"client_hello","protocol_version_min":1,"protocol_version_max":1000}
< {"kind":"server_hello","protocol_version":1}
"#).await;
}

#[tokio::test]
async fn test_server_version_selection_works_around() {
    one_client().conversation(r#"
> {"kind":"client_hello","protocol_version_min":0,"protocol_version_max":1000}
< {"kind":"server_hello","protocol_version":1}
"#).await;
}

#[tokio::test]
async fn test_client_smoke() {
    server().conversation(r#"
< {"kind":"client_hello","protocol_version_min":1,"protocol_version_max":1}
> {"kind":"server_hello","protocol_version":1}
< {"kind":"subscribe_bans"}
> {"kind":"replace_bans","bans":[]}
> {"kind":"close"}
< {"kind":"close","error":"unexpected server close"}
> EOF
< EOF
"#).await;
}

#[tokio::test]
async fn test_client_notjson() {
    server().conversation(r#"
< {"kind":"client_hello","protocol_version_min":1,"protocol_version_max":1}
> not json
< {"kind":"close","error":"invalid message read: expected ident at line 1 column 2"}
< EOF
"#).await;
}
