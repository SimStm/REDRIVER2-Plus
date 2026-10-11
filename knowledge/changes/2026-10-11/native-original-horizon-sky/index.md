# Native original horizon sky

Owned sky bytes and horizon geometry are captured before legacy uploads; one asynchronous worker
converts immutable inputs. Generation/revision and current-view epochs reject stale publications
or poses. The dedicated native sky pass runs before world/actors with depth test/write disabled;
it consumes original camera rotation, sky offset and evaluated brightness without changing simulation.

The native sky path accepts the bounded 20-byte original horizon polygon record without relaxing
source-span checks. PixelHistory on the pre-fix Havana capture identified a bilinear phase error at
the original gray/sky texture-row boundary; a sky-only half-texel adjustment now applies for linear
sampling. Point sampling and world/car UVs are unchanged. The private converted GLBs retain raw texel
UVs scaled once by 1/256; external GLB/mod UV contracts remain future work.

The focused `NativeSkyMeshTests` passed 45 checks; all 22 inspector suites passed before the
type-5 horizon fix, followed by the focused sky rerun. Final `NativeRendererSmoke` passed 1330/0;
serial native/reference x64 builds passed. Corrected injected captures: Havana mission 52,
942 draws/0 debug messages; Chicago
ATTRACT.400, post-publication RenderDoc frame 1783, 280 draws/0 messages. Both show the submitted
80-quad/160-triangle sky and the earlier line is not apparent in the visible sky. Chicago showed
10,167 placements/193 selected, 11 cars and 44 wheels submitted, 6 pedestrians ready/1 submitted.
These are bounded views; no matched backend parity, complete city coverage or damaged-car image is
claimed. Exact ignored captures/logs are named `sky-havana52-seamfix-native-20261011.*` and
`sky-chicago-attract400-seamfix-poststream-native2-20261011.*` under
`src_rebuild/build_e00/native-performance/`. Pre-fix PixelHistory script/log are
`pixelhistory_sky_havana.py` and `.log` in that directory.
