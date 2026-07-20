// Increment 2 — Qt + GStreamer linkage smoke test.
//
// Proves the full toolchain builds and runs together: Qt 6 (UI), GStreamer
// (media), the Direct3D 11 hardware probe, MSVC, CMake, and Ninja. It opens a
// small window showing the Qt and GStreamer versions and the detected machine
// tier. No video pipeline yet — that is increment 3.
//
// Run with "--selftest" for a headless check that exits immediately (used to
// verify the build without blocking on the window).

#include "hardware/HardwareProbe.h"

#include <gst/gst.h>

#include <QApplication>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    // gst_init consumes any --gst-* arguments and initializes the library.
    gst_init(&argc, &argv);

    gchar* gstVerRaw = gst_version_string();
    const std::string gstVersion = gstVerRaw ? gstVerRaw : "unknown";
    if (gstVerRaw) g_free(gstVerRaw);

    const vms::MachineProfile profile = vms::ProbeMachine();

    bool selftest = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--selftest") selftest = true;
    }

    // Console echo (visible in the smoke test / --selftest).
    std::cout << "Qt " << QT_VERSION_STR << "\n"
              << gstVersion << "\n"
              << "cpu=" << profile.cpuBrand << "\n"
              << "tier=" << profile.tier << std::endl;

    QApplication app(argc, argv);

    const QString info = QString(
        "VMS Native — increment 2\n\n"
        "Qt:          %1\n"
        "GStreamer:   %2\n"
        "CPU:         %3\n"
        "Cores:       %4\n"
        "Tier:        %5")
        .arg(QT_VERSION_STR)
        .arg(QString::fromStdString(gstVersion))
        .arg(QString::fromStdString(profile.cpuBrand))
        .arg(profile.logicalCores)
        .arg(QString::fromStdString(profile.tier));

    QWidget window;
    window.setWindowTitle(QStringLiteral("VMS Native — increment 2"));
    auto* layout = new QVBoxLayout(&window);
    auto* label = new QLabel(info, &window);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(label);
    window.resize(460, 220);
    window.show();

    if (selftest) {
        // Headless verification: the window was constructed and shown; exit now
        // instead of entering the event loop so the run does not block.
        return 0;
    }
    return app.exec();
}
