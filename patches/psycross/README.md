# PsyCross patches

These patches carry REDRIVER2-Plus-specific PsyCross changes without requiring
a fork of the entire upstream submodule.

After cloning the repository, initialise the submodule and apply the patches:

```powershell
git submodule update --init --recursive
powershell -ExecutionPolicy Bypass -File scripts/apply_psycross_patches.ps1
```

`developer-overlay.patch` is based on PsyCross commit
`e56e4cde1c2b8a15e0d4e38b26cdd9202e0d17e6`. The script refuses to apply it to
another revision and succeeds harmlessly when the patch is already present.
