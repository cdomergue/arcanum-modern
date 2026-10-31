# Runtime package ABI 1

`runtime.rwp` begins with eight bytes `RWPKG01\0`, then u32 ABI (1),
package ID string, display-version string, u32 revision, u32 min ABI, u32 max ABI,
module string. All integers are little endian. A string is a u32 byte length
followed by UTF-8 bytes, without a terminator. IDs/map names are ASCII.

Next: u32 dependency count, followed by ID string + u32 minimum revision.
Next: u32 resource count, followed by relative path string + 32-byte SHA-256.
Resources live beside runtime.rwp. Paths use forward slashes; no absolute paths,
empty components, drive letters, symlinks or `..`. New resources may be under
objects/, scr/, dlg/ or proto/. Existing campaign resources cannot be replaced
by this first ABI. Object payloads are package-private and not global overrides.

Next: u32 operation count. Each operation contains, in this order:

1. ID string; u32 introduction revision, kind, policy.
2. Map string; u64 sector ID.
3. u32 selector; GUID string; u32 ordinal, prototype, native object type.
4. Base resource path string; 32-byte SHA-256 (all zero only for GUID targets/new objects/global state).
5. Field string; u32 index; u64 mask; three u64 before values; three u64 after values.
6. Payload relative path string (only add_object).

There are no trailing bytes. Hash the exact operation byte span to obtain its
immutable digest; hash the whole runtime file for the package digest. Counts,
string lengths and total size are bounded by shared/world_updates_format.h.

Kinds: add=1, move=2, field=3, flags=4, ground=5, roof=6, block=7,
hide=8, global=9. Selectors: none=0, GUID=1, static ordinal=2.
Policies: conditional=0, authoritative=1 (must be explicitly chosen).

For move, before/after are packed location, signed32 X offset, signed32 Y offset
(stored as their unsigned32 bit pattern). Only mobile containers and scenery,
same map and same sector, are supported initially; a moved-away target conflicts.
For cells, index=y*64+x (roof y*16+x) and before/after[0] is the value.
For scalar fields, field is a native symbolic field name and [0] is the value.
Flags use mask and only compare/update those bits. Hide targets scenery and sets
OF_OFF with light/shadow cleanup, preserving its identity and record order.
Global uses field global_var/global_flag/quest_state, index is the native ID,
and [0] is the guarded state conversion. Completed/failed quest state regression
must be rejected. An operation may not write inventory, GUID, prototype or type.

Revisions are cumulative: keep published operation IDs and bytes unchanged in
later revisions. New operations have a new ID and introduction revision. A
package may skip revisions because the complete operation history is present.
Dependencies order packages. Unordered overlapping write sets are rejected.
All resource bytes are immutable for their package/path across revisions; use
new resource IDs for changed scripts/dialogues and explicit attachment migrations.

The authoring JSON uses decimal strings for uint64 values. A package folder also
contains manifest.json for review. It is compiled and validated before installation.
The installer registers package paths in ArcanumModern/world-updates/index.mes;
each record value is a relative `package-id/revision` directory. Native loading
verifies every resource digest before allowing any operation to apply.
