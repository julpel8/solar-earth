# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3.1]

### Fixed
- Removed the spurious green seam that ran along the right limb of the Oceania
  globe (and any region reaching the antimeridian, such as Asia and North
  America). The source day texture (`scripts/assets/world_src`) ships with a
  ~3px white border; sampling it near ±180° longitude leaked white pixels that
  the day classifier treated as land. `gen_globes.py` now trims this uniform
  near-white frame before sampling, and all globe images were regenerated.

## [0.3.0]

### Changed
- Improved earth rendering performance.

[0.3.1]: https://github.com/julpel8/solar-earth/releases/tag/v0.3.1
[0.3.0]: https://github.com/julpel8/solar-earth/releases/tag/v0.3.0
