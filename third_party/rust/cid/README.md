# rust-cid

[![](https://img.shields.io/badge/made%20by-Protocol%20Labs-blue.svg?style=flat-square)](http://ipn.io)
[![](https://img.shields.io/badge/project-multiformats-blue.svg?style=flat-square)](https://github.com/multiformats/multiformats)
[![](https://img.shields.io/badge/freenode-%23ipfs-blue.svg?style=flat-square)](https://webchat.freenode.net/?channels=%23ipfs)
[![](https://img.shields.io/badge/readme%20style-standard-brightgreen.svg?style=flat-square)](https://github.com/RichardLitt/standard-readme)

[![Build Status](https://github.com/multiformats/rust-cid/workflows/build/badge.svg)](https://github.com/multiformats/rust-cid/actions)
[![Crates.io](https://img.shields.io/crates/v/cid?style=flat-square)](https://crates.io/crates/cid)
[![License](https://img.shields.io/crates/l/cid?style=flat-square)](LICENSE)
[![Documentation](https://docs.rs/cid/badge.svg?style=flat-square)](https://docs.rs/cid)
[![Dependency Status](https://deps.rs/repo/github/multiformats/rust-cid/status.svg)](https://deps.rs/repo/github/multiformats/rust-cid)
[![Coverage Status](https://img.shields.io/codecov/c/github/multiformats/rust-cid?style=flat-square)](https://codecov.io/gh/multiformats/rust-cid)

> [CID](https://github.com/ipld/cid) implementation in Rust.

## Table of Contents

- [Install](#install)
- [Usage](#usage)
- [Maintainers](#maintainers)
- [Contribute](#contribute)
- [License](#license)

## Install

First add this to your `Cargo.toml`

```toml
[dependencies]
cid = "*"
multihash = "0.10"
```

Then run `cargo build`.

## Usage

```rust
use cid::{Cid, Codec, Version};
use multihash::Sha2_256;
use std::convert::TryFrom;

fn main() {
    let h = Sha2_256::digest(b"beep boop");

    let cid = Cid::new(Version::V1, Codec::DagProtobuf, h).unwrap();

    let data = cid.to_bytes();
    let out = Cid::try_from(data).unwrap();

    assert_eq!(cid, out);

    let cid_string = cid.to_string();
    /// bafybeieq5jui4j25lacwomsqgjeswwl3y5zcdrresptwgmfylxo2depppq
}
```
## Maintainers

Captain: [@dignifiedquire](https://github.com/dignifiedquire).

## Contribute

Contributions welcome. Please check out [the issues](https://github.com/multiformats/rust-cid/issues).

Check out our [contributing document](https://github.com/multiformats/multiformats/blob/master/contributing.md) for more information on how we work, and about contributing in general. Please be aware that all interactions related to multiformats are subject to the IPFS [Code of Conduct](https://github.com/ipfs/community/blob/master/code-of-conduct.md).

Small note: If editing the README, please conform to the [standard-readme](https://github.com/RichardLitt/standard-readme) specification.


## License

[MIT](LICENSE) © 2017 Friedel Ziegelmayer
