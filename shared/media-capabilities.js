(function mediaCapabilitiesModule(root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) module.exports = api;
  else root.VmsMediaCapabilities = api;
})(typeof globalThis !== "undefined" ? globalThis : this, function createMediaCapabilitiesModule() {
  "use strict";

  const CONTRACT_VERSION = 1;
  const CAPABILITY_KINDS = Object.freeze([
    "protocol",
    "profile",
    "container",
    "video-codec",
    "image-format",
    "audio-codec"
  ]);
  const MEDIA_OPERATIONS = Object.freeze([
    "discover",
    "connect",
    "demux",
    "decode",
    "encode",
    "record",
    "playback",
    "export",
    "transcode"
  ]);
  const SUPPORT_STATUSES = Object.freeze({
    SUPPORTED: "supported",
    UNSUPPORTED: "unsupported",
    UNAVAILABLE: "unavailable",
    UNVERIFIED: "unverified"
  });

  class MediaCapabilityError extends Error {
    constructor(code, message) {
      super(message);
      this.name = "MediaCapabilityError";
      this.code = code;
    }
  }

  function normalizeToken(value, field) {
    const token = String(value || "").trim().toLowerCase();
    if (!/^[a-z0-9][a-z0-9._:+-]*$/.test(token)) {
      throw new MediaCapabilityError("INVALID_CAPABILITY_TOKEN", `Invalid ${field}: ${value || "(empty)"}.`);
    }
    return token;
  }

  function normalizeKind(value) {
    const kind = normalizeToken(value, "capability kind");
    if (!CAPABILITY_KINDS.includes(kind)) {
      throw new MediaCapabilityError("INVALID_CAPABILITY_KIND", `Unknown capability kind: ${kind}.`);
    }
    return kind;
  }

  function normalizeOperation(value) {
    const operation = normalizeToken(value, "media operation");
    if (!MEDIA_OPERATIONS.includes(operation)) {
      throw new MediaCapabilityError("INVALID_MEDIA_OPERATION", `Unknown media operation: ${operation}.`);
    }
    return operation;
  }

  function capabilityKey(kind, id) {
    return `${normalizeKind(kind)}:${normalizeToken(id, "capability id")}`;
  }

  function freezeDescriptor(input) {
    const kind = normalizeKind(input?.kind);
    const id = normalizeToken(input?.id, "capability id");
    const aliases = [...new Set((input.aliases || []).map((alias) => normalizeToken(alias, "capability alias")))]
      .filter((alias) => alias !== id);
    const standards = (input.standards || []).map((standard) => Object.freeze({
      name: String(standard?.name || "").trim(),
      uri: String(standard?.uri || "").trim()
    }));
    if (!String(input?.name || "").trim()) {
      throw new MediaCapabilityError("CAPABILITY_NAME_REQUIRED", `Capability ${kind}:${id} requires a name.`);
    }
    if (standards.some((standard) => !standard.name || !/^https:\/\//.test(standard.uri))) {
      throw new MediaCapabilityError("INVALID_STANDARD_REFERENCE", `Capability ${kind}:${id} has an invalid standard reference.`);
    }
    return Object.freeze({
      contractVersion: CONTRACT_VERSION,
      kind,
      id,
      key: `${kind}:${id}`,
      name: String(input.name).trim(),
      aliases: Object.freeze(aliases),
      standards: Object.freeze(standards),
      deprecated: Boolean(input.deprecated),
      extension: Boolean(input.extension)
    });
  }

  function unknownResult(kind, id, operation) {
    return Object.freeze({
      contractVersion: CONTRACT_VERSION,
      kind: normalizeKind(kind),
      id: normalizeToken(id, "capability id"),
      operation: normalizeOperation(operation),
      status: SUPPORT_STATUSES.UNVERIFIED,
      reasonCode: "CAPABILITY_NOT_REGISTERED",
      evidence: null
    });
  }

  function normalizeEvidence(evidence) {
    if (!evidence || typeof evidence !== "object") return null;
    const provider = String(evidence.provider || "").trim();
    const probe = String(evidence.probe || "").trim();
    const testedAt = String(evidence.testedAt || "").trim();
    if (!provider || !probe || !testedAt || Number.isNaN(Date.parse(testedAt))) {
      throw new MediaCapabilityError(
        "INVALID_CAPABILITY_EVIDENCE",
        "Capability evidence requires provider, probe, and an ISO-compatible testedAt timestamp."
      );
    }
    return Object.freeze({
      provider,
      probe,
      testedAt: new Date(testedAt).toISOString(),
      version: String(evidence.version || "").trim(),
      device: String(evidence.device || "").trim(),
      notes: String(evidence.notes || "").trim()
    });
  }

  class MediaCapabilityRegistry {
    constructor(descriptors = []) {
      this.descriptors = new Map();
      this.aliases = new Map();
      this.reports = new Map();
      descriptors.forEach((descriptor) => this.register(descriptor));
    }

    register(input) {
      const descriptor = freezeDescriptor(input);
      if (this.descriptors.has(descriptor.key)) {
        throw new MediaCapabilityError("DUPLICATE_CAPABILITY", `Capability already registered: ${descriptor.key}.`);
      }
      this.descriptors.set(descriptor.key, descriptor);
      [descriptor.id, ...descriptor.aliases].forEach((alias) => {
        const aliasKey = capabilityKey(descriptor.kind, alias);
        const existing = this.aliases.get(aliasKey);
        if (existing && existing !== descriptor.key) {
          throw new MediaCapabilityError("DUPLICATE_CAPABILITY_ALIAS", `Capability alias already registered: ${aliasKey}.`);
        }
        this.aliases.set(aliasKey, descriptor.key);
      });
      return descriptor;
    }

    resolve(kind, id) {
      const key = this.aliases.get(capabilityKey(kind, id));
      return key ? this.descriptors.get(key) : null;
    }

    list(kind) {
      const normalizedKind = kind == null ? null : normalizeKind(kind);
      return Object.freeze([...this.descriptors.values()]
        .filter((descriptor) => !normalizedKind || descriptor.kind === normalizedKind)
        .sort((left, right) => left.key.localeCompare(right.key)));
    }

    report(input) {
      const descriptor = this.resolve(input?.kind, input?.id);
      if (!descriptor) {
        throw new MediaCapabilityError("CAPABILITY_NOT_REGISTERED", `Cannot report an unregistered capability: ${input?.kind}:${input?.id}.`);
      }
      const operation = normalizeOperation(input?.operation);
      const status = normalizeToken(input?.status, "support status");
      if (!Object.values(SUPPORT_STATUSES).includes(status)) {
        throw new MediaCapabilityError("INVALID_SUPPORT_STATUS", `Unknown support status: ${status}.`);
      }
      const evidence = input.evidence ? normalizeEvidence(input.evidence) : null;
      if (status === SUPPORT_STATUSES.SUPPORTED && !evidence) {
        throw new MediaCapabilityError(
          "SUPPORTED_EVIDENCE_REQUIRED",
          `Supported status requires runtime or conformance evidence for ${descriptor.key}/${operation}.`
        );
      }
      const report = Object.freeze({
        contractVersion: CONTRACT_VERSION,
        kind: descriptor.kind,
        id: descriptor.id,
        operation,
        status,
        reasonCode: String(input.reasonCode || "").trim(),
        evidence
      });
      this.reports.set(`${descriptor.key}/${operation}`, report);
      return report;
    }

    support(kind, id, operation) {
      const descriptor = this.resolve(kind, id);
      if (!descriptor) return unknownResult(kind, id, operation);
      return this.reports.get(`${descriptor.key}/${normalizeOperation(operation)}`) || Object.freeze({
        contractVersion: CONTRACT_VERSION,
        kind: descriptor.kind,
        id: descriptor.id,
        operation: normalizeOperation(operation),
        status: SUPPORT_STATUSES.UNVERIFIED,
        reasonCode: "CAPABILITY_NOT_PROBED",
        evidence: null
      });
    }

    negotiate({ kind, operation, candidates = [] }) {
      const normalizedKind = normalizeKind(kind);
      const normalizedOperation = normalizeOperation(operation);
      const considered = [];
      for (const candidate of candidates) {
        const id = typeof candidate === "string" ? candidate : candidate?.id;
        const remoteStatus = typeof candidate === "string"
          ? SUPPORT_STATUSES.SUPPORTED
          : String(candidate?.status || SUPPORT_STATUSES.UNVERIFIED).toLowerCase();
        const local = this.support(normalizedKind, id, normalizedOperation);
        considered.push(Object.freeze({ id: local.id, localStatus: local.status, remoteStatus }));
        if (local.status === SUPPORT_STATUSES.SUPPORTED && remoteStatus === SUPPORT_STATUSES.SUPPORTED) {
          return Object.freeze({
            status: "selected",
            kind: normalizedKind,
            operation: normalizedOperation,
            selectedId: local.id,
            considered: Object.freeze(considered)
          });
        }
      }
      return Object.freeze({
        status: "no-match",
        kind: normalizedKind,
        operation: normalizedOperation,
        selectedId: null,
        reasonCode: "NO_VERIFIED_CAPABILITY_INTERSECTION",
        considered: Object.freeze(considered)
      });
    }
  }

  const STANDARD_CATALOG = Object.freeze([
    { kind: "protocol", id: "rtsp-1", name: "Real-Time Streaming Protocol 1.0", aliases: ["rtsp"], standards: [{ name: "IETF RFC 2326", uri: "https://www.rfc-editor.org/rfc/rfc2326.html" }] },
    { kind: "protocol", id: "rtsp-2", name: "Real-Time Streaming Protocol 2.0", standards: [{ name: "IETF RFC 7826", uri: "https://www.rfc-editor.org/rfc/rfc7826.html" }] },
    { kind: "protocol", id: "rtp", name: "Real-time Transport Protocol", standards: [{ name: "IETF RFC 3550", uri: "https://www.rfc-editor.org/rfc/rfc3550.html" }] },
    { kind: "protocol", id: "srtp", name: "Secure Real-time Transport Protocol", standards: [{ name: "IETF RFC 3711", uri: "https://www.rfc-editor.org/rfc/rfc3711.html" }] },
    { kind: "protocol", id: "hls", name: "HTTP Live Streaming", standards: [{ name: "IETF RFC 8216", uri: "https://www.rfc-editor.org/rfc/rfc8216.html" }] },
    { kind: "protocol", id: "webrtc", name: "Web Real-Time Communication", standards: [{ name: "W3C WebRTC", uri: "https://www.w3.org/TR/webrtc/" }] },
    { kind: "protocol", id: "mpeg-dash", name: "Dynamic Adaptive Streaming over HTTP", aliases: ["dash"], standards: [{ name: "ISO/IEC 23009-1", uri: "https://www.iso.org/standard/83314.html" }] },
    { kind: "profile", id: "onvif-s", name: "ONVIF Profile S", standards: [{ name: "ONVIF Profile S", uri: "https://www.onvif.org/profiles/profile-s/" }] },
    { kind: "profile", id: "onvif-t", name: "ONVIF Profile T", standards: [{ name: "ONVIF Profile T", uri: "https://www.onvif.org/profiles/profile-t/" }] },
    { kind: "profile", id: "onvif-g", name: "ONVIF Profile G", standards: [{ name: "ONVIF Profile G", uri: "https://www.onvif.org/profiles/profile-g/" }] },
    { kind: "profile", id: "onvif-m", name: "ONVIF Profile M", standards: [{ name: "ONVIF Profile M", uri: "https://www.onvif.org/profiles/profile-m/" }] },
    { kind: "container", id: "isobmff", name: "ISO Base Media File Format", aliases: ["mp4"], standards: [{ name: "ISO/IEC 14496-12", uri: "https://www.iso.org/standard/83102.html" }] },
    { kind: "container", id: "mpeg-ts", name: "MPEG Transport Stream", aliases: ["ts"], standards: [{ name: "ISO/IEC 13818-1", uri: "https://www.iso.org/standard/87619.html" }] },
    { kind: "container", id: "matroska", name: "Matroska", aliases: ["mkv"], standards: [{ name: "IETF Matroska", uri: "https://datatracker.ietf.org/doc/draft-ietf-cellar-matroska/" }] },
    { kind: "container", id: "webm", name: "WebM", standards: [{ name: "WebM Container Guidelines", uri: "https://www.webmproject.org/docs/container/" }] },
    { kind: "video-codec", id: "h264", name: "Advanced Video Coding", aliases: ["avc"], standards: [{ name: "ITU-T H.264", uri: "https://www.itu.int/rec/T-REC-H.264" }] },
    { kind: "video-codec", id: "h265", name: "High Efficiency Video Coding", aliases: ["hevc"], standards: [{ name: "ITU-T H.265", uri: "https://www.itu.int/rec/T-REC-H.265" }] },
    { kind: "video-codec", id: "av1", name: "AOMedia Video 1", standards: [{ name: "AV1 Bitstream and Decoding Process", uri: "https://aomediacodec.github.io/av1-spec/" }] },
    { kind: "video-codec", id: "vp8", name: "VP8", standards: [{ name: "IETF RFC 6386", uri: "https://www.rfc-editor.org/rfc/rfc6386.html" }] },
    { kind: "video-codec", id: "vp9", name: "VP9", standards: [{ name: "IETF RFC 9628", uri: "https://www.rfc-editor.org/rfc/rfc9628.html" }] },
    { kind: "video-codec", id: "mpeg2-video", name: "MPEG-2 Video", standards: [{ name: "ISO/IEC 13818-2", uri: "https://www.iso.org/standard/61152.html" }] },
    { kind: "video-codec", id: "mjpeg", name: "Motion JPEG", aliases: ["motion-jpeg"], standards: [{ name: "IETF RFC 2435", uri: "https://www.rfc-editor.org/rfc/rfc2435.html" }] },
    { kind: "image-format", id: "jpeg", name: "JPEG", aliases: ["jpg"], standards: [{ name: "ISO/IEC 10918-1", uri: "https://www.iso.org/standard/75881.html" }] },
    { kind: "image-format", id: "png", name: "Portable Network Graphics", standards: [{ name: "W3C PNG Third Edition", uri: "https://www.w3.org/TR/png-3/" }] },
    { kind: "image-format", id: "webp", name: "WebP", standards: [{ name: "WebP Container Specification", uri: "https://developers.google.com/speed/webp/docs/riff_container" }] },
    { kind: "image-format", id: "avif", name: "AV1 Image File Format", standards: [{ name: "AOMedia AV1 Image File Format", uri: "https://aomediacodec.github.io/av1-avif/" }] },
    { kind: "image-format", id: "gif", name: "Graphics Interchange Format", standards: [{ name: "GIF89a", uri: "https://www.w3.org/Graphics/GIF/spec-gif89a.txt" }] },
    { kind: "image-format", id: "tiff", name: "Tagged Image File Format", aliases: ["tif"], standards: [{ name: "TIFF 6.0", uri: "https://www.loc.gov/preservation/digital/formats/fdd/fdd000022.shtml" }] },
    { kind: "image-format", id: "bmp", name: "Windows Bitmap", standards: [{ name: "Microsoft Open Specifications", uri: "https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-wmf/" }] },
    { kind: "audio-codec", id: "aac", name: "Advanced Audio Coding", standards: [{ name: "ISO/IEC 14496-3", uri: "https://www.iso.org/standard/81599.html" }] },
    { kind: "audio-codec", id: "opus", name: "Opus", standards: [{ name: "IETF RFC 6716", uri: "https://www.rfc-editor.org/rfc/rfc6716.html" }] },
    { kind: "audio-codec", id: "g711-ulaw", name: "G.711 mu-law", aliases: ["pcmu"], standards: [{ name: "ITU-T G.711", uri: "https://www.itu.int/rec/T-REC-G.711" }] },
    { kind: "audio-codec", id: "g711-alaw", name: "G.711 A-law", aliases: ["pcma"], standards: [{ name: "ITU-T G.711", uri: "https://www.itu.int/rec/T-REC-G.711" }] },
    { kind: "audio-codec", id: "pcm", name: "Linear PCM", standards: [{ name: "IETF RFC 3190", uri: "https://www.rfc-editor.org/rfc/rfc3190.html" }] }
  ].map(freezeDescriptor));

  function createStandardsRegistry() {
    return new MediaCapabilityRegistry(STANDARD_CATALOG);
  }

  return Object.freeze({
    CAPABILITY_KINDS,
    CONTRACT_VERSION,
    MEDIA_OPERATIONS,
    MediaCapabilityError,
    MediaCapabilityRegistry,
    STANDARD_CATALOG,
    SUPPORT_STATUSES,
    capabilityKey,
    createStandardsRegistry
  });
});
