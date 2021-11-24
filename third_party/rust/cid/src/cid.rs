//! This module contains the main CID type.
//!
//! If you are an application developer you likely won't use the `Cid` which is generic over the
//! digest size. Intead you would use the concrete top-level `Cid` type.
//!
//! As a library author that works with CIDs that should support hashes of anysize, you would
//! import the `Cid` type from this module.
#[cfg(feature = "std")]
use std::convert::TryFrom;

#[cfg(feature = "std")]
use multibase::{encode as base_encode, Base};
use multihash::{MultihashGeneric as Multihash, Size};
#[cfg(feature = "std")]
use unsigned_varint::{encode as varint_encode, io::read_u64 as varint_read_u64};

use crate::error::{Error, Result};
use crate::version::Version;

/// DAG-PB multicodec code
const DAG_PB: u64 = 0x70;
/// The SHA_256 multicodec code
const SHA2_256: u64 = 0x12;

/// Representation of a CID.
///
/// The generic is about the allocated size of the multihash.
#[derive(PartialEq, Eq, Clone, PartialOrd, Ord, Hash)]
#[cfg_attr(feature = "scale-codec", derive(parity_scale_codec::Decode))]
#[cfg_attr(feature = "scale-codec", derive(parity_scale_codec::Encode))]
#[cfg_attr(feature = "serde-codec", derive(serde::Deserialize))]
#[cfg_attr(feature = "serde-codec", derive(serde::Serialize))]
#[cfg_attr(feature = "serde-codec", serde(bound = "S: Size"))]
pub struct Cid<S: Size> {
    /// The version of CID.
    version: Version,
    /// The codec of CID.
    codec: u64,
    /// The multihash of CID.
    hash: Multihash<S>,
}

impl<S: Size> Copy for Cid<S> where S::ArrayType: Copy {}

impl<S: Size> Cid<S> {
    /// Create a new CIDv0.
    pub fn new_v0(hash: Multihash<S>) -> Result<Self> {
        if hash.code() != SHA2_256 {
            return Err(Error::InvalidCidV0Multihash);
        }
        Ok(Self {
            version: Version::V0,
            codec: DAG_PB,
            hash,
        })
    }

    /// Create a new CIDv1.
    pub fn new_v1(codec: u64, hash: Multihash<S>) -> Self {
        Self {
            version: Version::V1,
            codec,
            hash,
        }
    }

    /// Create a new CID.
    pub fn new(version: Version, codec: u64, hash: Multihash<S>) -> Result<Self> {
        match version {
            Version::V0 => {
                if codec != DAG_PB {
                    return Err(Error::InvalidCidV0Codec);
                }
                Self::new_v0(hash)
            }
            Version::V1 => Ok(Self::new_v1(codec, hash)),
        }
    }

    /// Returns the cid version.
    pub fn version(&self) -> Version {
        self.version
    }

    /// Returns the cid codec.
    pub fn codec(&self) -> u64 {
        self.codec
    }

    /// Returns the cid multihash.
    pub fn hash(&self) -> &Multihash<S> {
        &self.hash
    }

    /// Reads the bytes from a byte stream.
    #[cfg(feature = "std")]
    pub fn read_bytes<R: std::io::Read>(mut r: R) -> Result<Self> {
        let version = varint_read_u64(&mut r)?;
        let codec = varint_read_u64(&mut r)?;
        // CIDv0 has the fixed `0x12 0x20` prefix
        if [version, codec] == [0x12, 0x20] {
            let mut digest = [0u8; 32];
            r.read_exact(&mut digest)?;
            let mh = Multihash::wrap(version, &digest).expect("Digest is always 32 bytes.");
            Self::new_v0(mh)
        } else {
            let version = Version::try_from(version)?;
            let mh = Multihash::read(r)?;
            Self::new(version, codec, mh)
        }
    }

    #[cfg(feature = "std")]
    fn write_bytes_v1<W: std::io::Write>(&self, mut w: W) -> Result<()> {
        let mut version_buf = varint_encode::u64_buffer();
        let version = varint_encode::u64(self.version.into(), &mut version_buf);

        let mut codec_buf = varint_encode::u64_buffer();
        let codec = varint_encode::u64(self.codec, &mut codec_buf);

        w.write_all(version)?;
        w.write_all(codec)?;
        self.hash.write(&mut w)?;
        Ok(())
    }

    /// Writes the bytes to a byte stream.
    #[cfg(feature = "std")]
    pub fn write_bytes<W: std::io::Write>(&self, w: W) -> Result<()> {
        match self.version {
            Version::V0 => self.hash.write(w)?,
            Version::V1 => self.write_bytes_v1(w)?,
        }
        Ok(())
    }

    /// Returns the encoded bytes of the `Cid`.
    #[cfg(feature = "std")]
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut bytes = vec![];
        self.write_bytes(&mut bytes).unwrap();
        bytes
    }

    #[cfg(feature = "std")]
    fn to_string_v0(&self) -> String {
        Base::Base58Btc.encode(self.hash.to_bytes())
    }

    #[cfg(feature = "std")]
    fn to_string_v1(&self) -> String {
        multibase::encode(Base::Base32Lower, self.to_bytes().as_slice())
    }

    /// Convert CID into a multibase encoded string
    ///
    /// # Example
    ///
    /// ```
    /// use cid::Cid;
    /// use multibase::Base;
    /// use multihash::{Code, MultihashDigest};
    ///
    /// const RAW: u64 = 0x55;
    ///
    /// let cid = Cid::new_v1(RAW, Code::Sha2_256.digest(b"foo"));
    /// let encoded = cid.to_string_of_base(Base::Base64).unwrap();
    /// assert_eq!(encoded, "mAVUSICwmtGto/8aP+ZtFPB0wQTQTQi1wZIO/oPmKXohiZueu");
    /// ```
    #[cfg(feature = "std")]
    pub fn to_string_of_base(&self, base: Base) -> Result<String> {
        match self.version {
            Version::V0 => {
                if base == Base::Base58Btc {
                    Ok(self.to_string_v0())
                } else {
                    Err(Error::InvalidCidV0Base)
                }
            }
            Version::V1 => Ok(base_encode(base, self.to_bytes())),
        }
    }
}

impl<S: Size> Default for Cid<S> {
    fn default() -> Self {
        Self {
            version: Version::V1,
            codec: 0,
            hash: Multihash::<S>::default(),
        }
    }
}

#[cfg(feature = "std")]
impl<S: Size> std::fmt::Display for Cid<S> {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        let output = match self.version {
            Version::V0 => self.to_string_v0(),
            Version::V1 => self.to_string_v1(),
        };
        write!(f, "{}", output)
    }
}

#[cfg(feature = "std")]
impl<S: Size> std::fmt::Debug for Cid<S> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if f.alternate() {
            f.debug_struct("Cid")
                .field("version", &self.version())
                .field("codec", &self.codec())
                .field("hash", self.hash())
                .finish()
        } else {
            let output = match self.version {
                Version::V0 => self.to_string_v0(),
                Version::V1 => self.to_string_v1(),
            };
            write!(f, "Cid({})", output)
        }
    }
}

#[cfg(feature = "std")]
impl<S: Size> std::str::FromStr for Cid<S> {
    type Err = Error;

    fn from_str(cid_str: &str) -> Result<Self> {
        Self::try_from(cid_str)
    }
}

#[cfg(feature = "std")]
impl<S: Size> TryFrom<String> for Cid<S> {
    type Error = Error;

    fn try_from(cid_str: String) -> Result<Self> {
        Self::try_from(cid_str.as_str())
    }
}

#[cfg(feature = "std")]
impl<S: Size> TryFrom<&str> for Cid<S> {
    type Error = Error;

    fn try_from(cid_str: &str) -> Result<Self> {
        static IPFS_DELIMETER: &str = "/ipfs/";

        let hash = match cid_str.find(IPFS_DELIMETER) {
            Some(index) => &cid_str[index + IPFS_DELIMETER.len()..],
            _ => cid_str,
        };

        if hash.len() < 2 {
            return Err(Error::InputTooShort);
        }

        let decoded = if Version::is_v0_str(hash) {
            Base::Base58Btc.decode(hash)?
        } else {
            let (_, decoded) = multibase::decode(hash)?;
            decoded
        };

        Self::try_from(decoded)
    }
}

#[cfg(feature = "std")]
impl<S: Size> TryFrom<Vec<u8>> for Cid<S> {
    type Error = Error;

    fn try_from(bytes: Vec<u8>) -> Result<Self> {
        Self::try_from(bytes.as_slice())
    }
}

#[cfg(feature = "std")]
impl<S: Size> TryFrom<&[u8]> for Cid<S> {
    type Error = Error;

    fn try_from(mut bytes: &[u8]) -> Result<Self> {
        Self::read_bytes(&mut bytes)
    }
}

impl<S: Size> From<&Cid<S>> for Cid<S>
where
    S::ArrayType: Copy,
{
    fn from(cid: &Cid<S>) -> Self {
        *cid
    }
}

#[cfg(feature = "std")]
impl<S: Size> From<Cid<S>> for Vec<u8> {
    fn from(cid: Cid<S>) -> Self {
        cid.to_bytes()
    }
}

#[cfg(feature = "std")]
impl<S: Size> From<Cid<S>> for String {
    fn from(cid: Cid<S>) -> Self {
        cid.to_string()
    }
}

#[cfg(feature = "std")]
impl<'a, S: Size> From<Cid<S>> for std::borrow::Cow<'a, Cid<S>> {
    fn from(from: Cid<S>) -> Self {
        std::borrow::Cow::Owned(from)
    }
}

#[cfg(feature = "std")]
impl<'a, S: Size> From<&'a Cid<S>> for std::borrow::Cow<'a, Cid<S>> {
    fn from(from: &'a Cid<S>) -> Self {
        std::borrow::Cow::Borrowed(from)
    }
}

#[cfg(test)]
mod tests {
    #[test]
    #[cfg(feature = "scale-codec")]
    fn test_cid_scale_codec() {
        use super::Cid;
        use multihash::U64;
        use parity_scale_codec::{Decode, Encode};

        let cid = Cid::<U64>::default();
        let bytes = cid.encode();
        let cid2 = Cid::decode(&mut &bytes[..]).unwrap();
        assert_eq!(cid, cid2);
    }

    #[test]
    #[cfg(feature = "serde-codec")]
    fn test_cid_serde() {
        use super::Cid;
        use multihash::U64;

        let cid = Cid::<U64>::default();
        let bytes = serde_json::to_string(&cid).unwrap();
        let cid2 = serde_json::from_str(&bytes).unwrap();
        assert_eq!(cid, cid2);
    }

    #[test]
    #[cfg(feature = "std")]
    fn test_debug_instance() {
        use super::Cid;
        use multihash::U64;
        use std::str::FromStr;
        let cid =
            Cid::<U64>::from_str("bafyreibjo4xmgaevkgud7mbifn3dzp4v4lyaui4yvqp3f2bqwtxcjrdqg4")
                .unwrap();
        // short debug
        assert_eq!(
            &format!("{:?}", cid),
            "Cid(bafyreibjo4xmgaevkgud7mbifn3dzp4v4lyaui4yvqp3f2bqwtxcjrdqg4)"
        );
        // verbose debug
        let mut txt = format!("{:#?}", cid);
        txt.retain(|c| !c.is_whitespace());
        assert_eq!(&txt, "Cid{version:V1,codec:113,hash:Multihash{code:18,size:32,digest:[41,119,46,195,0,149,81,168,63,176,40,43,118,60,191,149,226,240,10,35,152,172,31,178,232,48,180,238,36,196,112,55,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,],},}");
    }
}
