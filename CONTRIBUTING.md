# Contributing to Viper

Thank you for your interest in contributing to Viper!

## Getting started

1. Fork the repository and clone your fork.
2. Build and run tests:

```bash
make clean && make all && make test && make verify
```

3. Make focused changes and ensure tests pass before opening a pull request.

## Pull requests

- Keep diffs small and focused on one change.
- Run `make test` and `make verify` before submitting.
- Update [docs/language-spec.md](docs/language-spec.md) if you change language behavior.
- Add or update tests when fixing bugs or adding features.

## Language changes

See [docs/language-spec.md](docs/language-spec.md) for the current language reference. Proposals for new syntax or semantics should include examples and test cases.

## Code style

- Match existing C style in the codebase (C11, `-Wall -Wextra -Werror`).
- Prefer minimal, readable changes over large refactors.

## License

By contributing, you agree that your contributions will be licensed under the [Apache License 2.0](LICENSE).
