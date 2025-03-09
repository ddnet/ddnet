use quinn::rustls::DigitallySignedStruct;
use quinn::rustls::Error;
use quinn::rustls::OtherError;
use quinn::rustls::RootCertStore;
use quinn::rustls::SignatureScheme;
use quinn::rustls::client::danger;
use quinn::rustls::crypto::WebPkiSupportedAlgorithms;
use quinn::rustls::crypto;
use quinn::rustls::pki_types::CertificateDer;
use quinn::rustls::pki_types::ServerName;
use quinn::rustls::pki_types::UnixTime;
use std::error;
use std::fmt;
use std::sync::Arc;

#[derive(Debug, Default)]
pub struct Verifier;

#[derive(Debug)]
struct ServerSentInvalidCertSignatureScheme;
impl error::Error for ServerSentInvalidCertSignatureScheme {}
impl fmt::Display for ServerSentInvalidCertSignatureScheme {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        "server sent invalid certificate signature scheme".fmt(f)
    }
}

static SUPPORTED_ALGORITHMS: WebPkiSupportedAlgorithms = WebPkiSupportedAlgorithms {
    all: &[webpki::ring::ED25519],
    mapping: &[(SignatureScheme::ED25519, &[webpki::ring::ED25519])],
};

impl danger::ServerCertVerifier for Verifier {
    fn verify_server_cert(
        &self,
        end_entity: &CertificateDer<'_>,
        intermediates: &[CertificateDer<'_>],
        server_name: &ServerName<'_>,
        ocsp_response: &[u8],
        now: UnixTime,
    ) -> Result<danger::ServerCertVerified, Error> {
        // TODO
        Ok(danger::ServerCertVerified::assertion())
    }
    fn verify_tls12_signature(
        &self,
        message: &[u8],
        cert: &CertificateDer<'_>,
        dss: &DigitallySignedStruct,
    ) -> Result<danger::HandshakeSignatureValid, Error> {
        let _ = (message, cert, dss);
        unimplemented!(); // not used in QUIC
    }
    fn verify_tls13_signature(
        &self,
        message: &[u8],
        cert: &CertificateDer<'_>,
        dss: &DigitallySignedStruct,
    ) -> Result<danger::HandshakeSignatureValid, Error> {
        crypto::verify_tls13_signature(
            message,
            cert,
            dss,
            &SUPPORTED_ALGORITHMS,
        )
    }
    fn supported_verify_schemes(&self) -> Vec<SignatureScheme> {
        SUPPORTED_ALGORITHMS.supported_schemes()
    }
}
