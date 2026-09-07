# Aggregate schema validation contract

`aggregate-record-v1.schema.json` is the canonical structural shape for a
persisted or transported Quiet Trace aggregate record. Standard JSON Schema
Draft 2020-12 validators enforce its types, ranges, required fields, closed
property sets, timestamp format, array length, and scalar bounds.

Two required aggregate invariants cannot be expressed portably in Draft
2020-12:

- all ten `histogram_counts` values must total exactly 480 complete 125 ms
  windows; and
- `peak_125ms_dbfs` must be greater than or equal to `level_eq_dbfs`.

The schema marks those rules with `x-quiet-trace-*` annotations and declares
`x-quiet-trace-semantic-validation.required` as `true`. Standard validators
normally ignore extension keywords, so accepting a record based only on
ordinary JSON Schema output is a contract violation.

All schema `minLength`/`maxLength` limits count Unicode scalar values, matching
JSON Schema semantics rather than UTF-8 bytes or UTF-16 code units. The C++
validator also rejects malformed UTF-8, and the TypeScript validator rejects
unpaired UTF-16 surrogates. This `valid_unicode_scalar_text` rule is part of the
required semantic validation because ordinary JavaScript JSON Schema tooling
can represent an unpaired surrogate. Timestamps use canonical UTC `Z` form with
seconds `00` through `59`; leap-second notation is intentionally rejected so
all implementations behave identically. Clock states `unknown` and
`monotonic_only` require a null wall timestamp; `host_set` and `synced` require
a canonical wall timestamp.

Every producer and consumer must run both:

1. structural JSON Schema validation; and
2. the platform semantic validator:
   - TypeScript: `parseAggregateRecord` in
     `app/src/contracts/aggregate-record.ts`
   - C++: `quiet_trace::is_valid` in
     `firmware/components/domain/aggregate_contract.cpp`

`app/tests/aggregate-schema.test.ts` deliberately runs an ordinary validator
to prove this boundary, then verifies that the semantic implementations reject
the same invalid records. Adding another cross-field or aggregate invariant
requires updating the schema metadata, both implementations, and parity tests.

## Dashboard API payload schemas (issue #5)

The dashboard also validates two versioned transport envelopes:

- `status-v1.schema.json` for `/api/v1/status`
- `records-page-v1.schema.json` for paginated `/api/v1/records`

Fixtures are committed under `docs/schemas/fixtures/` and are validated in
`app/tests/protocol-schema.test.ts` with AJV + TypeScript semantic parsers.

Unknown additive top-level fields are tolerated for forward compatibility
(the parser exposes them in `extensions`), but schema/version mismatch and
aggregate-content violations are hard failures.
