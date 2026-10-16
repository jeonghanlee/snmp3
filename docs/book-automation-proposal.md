# Proposal: automated build of the documentation book

This proposal describes how a continuous integration job can build the book with the same tool as the local build. It is a proposal only. The repository contains no workflow file, and nothing in this document publishes the book.

## Scope

This document covers the build and check steps of an automated job and the inputs it needs. It names the toolchain and the checks.

**Out of scope:** the hosting target, the publication step, and the credentials for either. Each needs a separate decision and its own observed result.

## Toolchain

The job uses the container image `jeonghanlee/mdbook:0.5.4` (mdBook 0.5.4, no plugin), the same tag as [Build the documentation book](book-build.md). The job pins the tag, so a later image cannot change the output.

## Proposed job steps

1. Check out the repository at the commit under test.
2. Run the build command of the local procedure in the container.
3. Remove the non-Markdown files that no page links to, as in the local procedure.
4. Fail the job when the build prints a warning, when a non-HTML file remains below `public/snmp-performance` or `public/powernet-460-review`, or when a checker finds a broken internal link or anchor in the rendered pages.
5. Store `public` as a job artifact for inspection.

## Link checker

The local check reads every rendered page, resolves each internal link against the output tree, and compares each anchor with the `id` attributes of the target page. The checked build has 296 internal links and no broken one. The repository does not ship this checker yet. Adding it as a tracked tool is part of the decision this proposal asks for.

## Decisions needed

- Whether the repository ships the link checker as a tracked tool.
- Which continuous integration system runs the job.
- Whether and where the book is published, and who authorizes it.
