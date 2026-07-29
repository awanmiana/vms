#include "PlaybackController.h"

#include <cstdio>
#include <ctime>

using vms::persist::AvailabilitySpan;
using vms::persist::ComputeAvailability;
using vms::persist::Segment;
using vms::persist::SpanState;
using vms::persist::SpanStateName;

long long ParseUtcSeconds(const QString& utc) {
    int Y = 0, Mo = 0, D = 0, h = 0, m = 0, s = 0;
    if (std::sscanf(utc.toUtf8().constData(), "%d-%d-%d %d:%d:%d",
                    &Y, &Mo, &D, &h, &m, &s) != 6)
        return -1;
    std::tm tm{};
    tm.tm_year = Y - 1900;
    tm.tm_mon = Mo - 1;
    tm.tm_mday = D;
    tm.tm_hour = h;
    tm.tm_min = m;
    tm.tm_sec = s;
#ifdef _WIN32
    return static_cast<long long>(_mkgmtime(&tm));
#else
    return static_cast<long long>(timegm(&tm));
#endif
}

PlaybackController::PlaybackController(vms::persist::SegmentIndex* index,
                                      QString cameraId, QObject* parent)
    : QObject(parent), index_(index), cameraId_(std::move(cameraId)) {}

long long PlaybackController::startSec() const { return ParseUtcSeconds(rangeStart_); }
long long PlaybackController::endSec() const { return ParseUtcSeconds(rangeEnd_); }

void PlaybackController::setRange(const QString& startUtc, const QString& endUtc) {
    rangeStart_ = startUtc;
    rangeEnd_ = endUtc;
    playheadSec_ = startSec();   // start at the beginning of the window
    reload();
    emit changed();
    emit playheadChanged();
}

void PlaybackController::refresh() {
    reload();
    emit changed();
}

void PlaybackController::reload() {
    spans_.clear();
    segs_.clear();
    if (!index_ || rangeStart_.isEmpty() || rangeEnd_.isEmpty()) return;

    index_->list(cameraId_.toStdString(), rangeStart_.toStdString(),
                 rangeEnd_.toStdString(), segs_);

    const std::vector<AvailabilitySpan> spans = ComputeAvailability(
        segs_, rangeStart_.toStdString(), rangeEnd_.toStdString());

    const long long a = startSec(), b = endSec();
    const double dur = (b > a) ? static_cast<double>(b - a) : 1.0;
    for (const AvailabilitySpan& s : spans) {
        const long long ss = ParseUtcSeconds(QString::fromStdString(s.startUtc));
        const long long se = ParseUtcSeconds(QString::fromStdString(s.endUtc));
        QVariantMap m;
        m.insert(QStringLiteral("state"), QString::fromLatin1(SpanStateName(s.state)));
        m.insert(QStringLiteral("startFrac"), (ss - a) / dur);
        m.insert(QStringLiteral("endFrac"), (se - a) / dur);
        m.insert(QStringLiteral("startUtc"), QString::fromStdString(s.startUtc));
        m.insert(QStringLiteral("endUtc"), QString::fromStdString(s.endUtc));
        m.insert(QStringLiteral("sources"), s.sources);
        spans_.push_back(m);
    }
}

double PlaybackController::playheadFrac() const {
    const long long a = startSec(), b = endSec();
    if (b <= a) return 0.0;
    double f = static_cast<double>(playheadSec_ - a) / static_cast<double>(b - a);
    if (f < 0.0) f = 0.0;
    if (f > 1.0) f = 1.0;
    return f;
}

QString PlaybackController::playheadUtc() const {
    std::time_t t = static_cast<std::time_t>(playheadSec_);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm);
    return QString::fromLatin1(buf);
}

bool PlaybackController::onFootage() const {
    for (const Segment& s : segs_) {
        const long long ss = ParseUtcSeconds(QString::fromStdString(s.startUtc));
        const long long se = ParseUtcSeconds(QString::fromStdString(s.endUtc));
        if (playheadSec_ >= ss && playheadSec_ < se) return true;
    }
    return false;
}

void PlaybackController::seekFrac(double frac) {
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;
    const long long a = startSec(), b = endSec();
    if (b <= a) return;
    playheadSec_ = a + static_cast<long long>(frac * static_cast<double>(b - a));
    emit playheadChanged();
}

void PlaybackController::setPlayheadAbs(long long epochSec) {
    const long long a = startSec(), b = endSec();
    if (epochSec < a) epochSec = a;
    if (epochSec > b) epochSec = b;
    if (epochSec == playheadSec_) return;
    playheadSec_ = epochSec;
    emit playheadChanged();
}

void PlaybackController::play() {
    if (!playing_) { playing_ = true; emit transportChanged(); }
}
void PlaybackController::pause() {
    if (playing_) { playing_ = false; emit transportChanged(); }
}
void PlaybackController::setSpeed(double speed) {
    if (speed <= 0.0) speed = 1.0;
    if (speed != speed_) { speed_ = speed; emit transportChanged(); }
}

void PlaybackController::stepFrames(int frames, int fps) {
    if (fps <= 0) fps = 25;
    // Approximate frame-step at 1s resolution (the index is second-granular in
    // this pass): move at least one second in the requested direction.
    long long deltaSec = frames / fps;
    if (deltaSec == 0) deltaSec = (frames >= 0) ? 1 : -1;
    playheadSec_ += deltaSec;
    const long long a = startSec(), b = endSec();
    if (playheadSec_ < a) playheadSec_ = a;
    if (playheadSec_ > b) playheadSec_ = b;
    emit playheadChanged();
}

QVariantMap PlaybackController::segmentAtPlayhead() const {
    QVariantMap out;
    out.insert(QStringLiteral("path"), QString());
    out.insert(QStringLiteral("offsetSec"), 0.0);
    for (const Segment& s : segs_) {
        const long long ss = ParseUtcSeconds(QString::fromStdString(s.startUtc));
        const long long se = ParseUtcSeconds(QString::fromStdString(s.endUtc));
        if (playheadSec_ >= ss && playheadSec_ < se) {
            out.insert(QStringLiteral("path"), QString::fromStdString(s.path));
            out.insert(QStringLiteral("offsetSec"),
                       static_cast<double>(playheadSec_ - ss));
            break;
        }
    }
    return out;
}

QString PlaybackController::dumpSpans() const {
    QString out;
    for (const QVariant& v : spans_) {
        const QVariantMap m = v.toMap();
        out += QStringLiteral("  [%1 .. %2] %3 (%4 src)\n")
                   .arg(m.value(QStringLiteral("startUtc")).toString())
                   .arg(m.value(QStringLiteral("endUtc")).toString())
                   .arg(m.value(QStringLiteral("state")).toString())
                   .arg(m.value(QStringLiteral("sources")).toInt());
    }
    return out;
}
