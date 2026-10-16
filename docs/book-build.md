# Build the documentation book

You build this book from the Markdown sources with mdBook in a container. The build copies every non-Markdown file under `docs` into the output, so a final step removes the files that no page links to.

## Prerequisites

- Docker, with the image `jeonghanlee/mdbook:0.5.4` available locally. The image carries mdBook 0.5.4 and no plugin.
- A checkout of the repository, with the current directory at its root.

## Steps

1. To build the book into `public`, run this command from the checkout root:

    ```bash
    docker run --rm -u "$(id -u):$(id -g)" -v "$PWD":/book -w /book jeonghanlee/mdbook:0.5.4 mdbook build
    ```

    The build reads `book.toml` and `docs/SUMMARY.md`. It prints no warning when every file in `SUMMARY.md` exists.

2. To remove the files that no page links to, run:

    ```bash
    rm -rf public/snmp-performance/tools public/snmp-performance/inputs public/snmp-performance/results public/powernet-460-review/evidence
    ```

    Pages link to the two data files, `inputs/ap8932-oids.tsv` and `results/reference-run.json`, through repository URLs. The remaining files are tools and evidence that the build copies from `docs`.

## Verification

Build output ends with these lines:

```text
 INFO Book building has started
 INFO Running the html backend
 INFO HTML book written to `/book/public`
```

List the non-HTML files that remain below the two page directories:

```bash
find public/snmp-performance public/powernet-460-review -type f ! -name '*.html'
```

The command prints nothing. Then open `public/index.html` in a browser and follow the navigation to each page. The directory `public` is excluded from source tracking.
