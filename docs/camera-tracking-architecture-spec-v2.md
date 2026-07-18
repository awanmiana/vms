# Camera Tracking Architecture v2

This revision replaces confidence-ranked camera links as the primary navigation model. The core idea is spatial navigation: cameras live on a site canvas at their real approximate positions, and the operator moves through the premises by panning, zooming, selecting pins, and opening the camera physically closest to the subject's last visible movement.

## Primary Layers

1. Spatial canvas
   - One canvas per premises, floor, or large zone.
   - Each camera has a stored canvas position, facing direction, and field-of-view angle.
   - The operator can pan/zoom the site as a live navigation surface.

2. Viewport stream policy
   - Focus-zone cameras get higher-quality streams.
   - Peripheral cameras get thumbnail or substream previews.
   - Off-screen cameras pause after a short grace period.
   - A pre-warm ring opens likely nearby streams just outside the viewport during pan movement to reduce blank startup time.
   - In group live view, cameras render as full tiles on a draggable grid, not as map dots. Off-screen tiles are culled; tiles near the viewport center become prominent stream candidates.
   - In physical site-map mode, camera pins can remain visible as map context; expensive stream/video cards should still be virtualized by viewport zone.

3. Command interface
   - Every action becomes the same `VMSCommand` shape before execution.
   - UI clicks, map gestures, regex-parsed voice, and the future voice agent all use the same executor.
   - This prevents voice control from becoming a separate fragile feature.

4. Tracking sessions
   - A case has a status, priority, subject descriptor, and breadcrumbs.
   - Breadcrumbs record camera, time, command source, and canvas position.
   - Paused cases can be resumed by returning the viewport to the last breadcrumb.

5. Voice v1
   - STT transcript is normalized.
   - Regex rules produce commands.
   - Voice aliases map speakable phrases like "front door" to formal camera/group/zone records.
   - The later voice agent only needs to emit the same command object.

## Why This Fits The Operator Problem

The app should not pretend it can always predict a person's next camera. In open areas, that prediction is weak. Instead, the system should make the operator faster by showing the physical camera neighborhood, warming nearby streams, remembering case breadcrumbs, and letting voice or gestures trigger the same reliable actions.

Historical route confidence can still exist later as a light hint, but it should not be the foundation of tracking.
