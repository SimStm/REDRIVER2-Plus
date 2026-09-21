# Developer settings files

`developer_graphics.ini`, `developer_input.ini`, `developer_modern_mesh.ini` and
`developer_debug_start.ini` hold developer state that sits on top of the shipped
`config.ini`. The panel never writes `config.ini`; a developer value is applied
after the shipped one, so it wins, except for `textureOverrides`, which forces
the shipped behaviour for reproducible captures when the key is present.

New and changed settings files go through
`DeveloperSettingsFile_WriteKeys(path, comments, commentCount, knownKeys,
knownKeyCount, keys, values, keyCount)` (`src_rebuild/utils/DeveloperSettingsFile.{h,cpp}`)
instead of writing a `fprintf` block:

- it rewrites the file in place, so keys the caller does not own - unknown keys,
  comments, blank lines from another tool or a later version - are preserved
  verbatim;
- a key that is in `knownKeys` but not in `keys` is **removed**, which is how an
  owner drops a value it no longer wants; keys that are missing are appended in
  the given order;
- the new contents go to `<path>.tmp` and replace the real file only when the
  write succeeded, rotating the previous contents into `<path>.bak`; a failure
  removes the temporary file and leaves the existing file untouched.

Persist only what differs from the shipped default when the setting is a
default-plus-overrides model (input bindings do this): the file stays small, and
deleting a line or pressing Reset returns the shipped value. Keys are stable
identifiers - renaming one silently discards the user's value - and loading must
validate ranges before applying a value, because the file is user-editable.
