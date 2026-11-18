# ClassCaption

Live lecture captions that run entirely on a Mac, with Wi-Fi off.

Work in progress. The sections below fill in as each part is built and measured.

## Build

```
brew install cmake llvm
git clone --recurse-submodules <repo-url>
cd ClassCaption
scripts/fetch_models.sh
scripts/check.sh
```

`scripts/check.sh` configures, builds, runs the tests, and runs clang-format and clang-tidy. CI runs the same script.
