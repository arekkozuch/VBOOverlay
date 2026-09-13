#pragma once

#include <QGuiApplication>

namespace FlappedEar::ApplicationIdentity {

inline constexpr auto displayName = "Flapped Ear Telemetry";
inline constexpr auto storageName = "FlappedEar Telemetry";
inline constexpr auto organization = "FlappedEar";
inline constexpr auto domain = "flappedear.com";

inline void initialize()
{
    // These identifiers own existing QSettings and QStandardPaths data. A
    // display/bundle rename must never create a second preferences/recovery tree.
    QCoreApplication::setOrganizationName(organization);
    QCoreApplication::setOrganizationDomain(domain);
    QCoreApplication::setApplicationName(storageName);
    QGuiApplication::setApplicationDisplayName(displayName);
}

} // namespace FlappedEar::ApplicationIdentity
