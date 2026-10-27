#include "PackageSelectProcess.h"
#include "GlobalStorage.h"
#include "JobQueue.h"
#include <QProcess>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QFile>

// This is the main implementation of the Calamares job responsible for
// installing the user-selected packages (debs + snaps) during the
// target system setup phase.

CALAMARES_PLUGIN_FACTORY_DEFINITION(PackageSelectProcessFactory, registerPlugin<PackageSelectProcess>();)

PackageSelectProcess::PackageSelectProcess(QObject* parent)
    : Calamares::CppJob(parent),
      m_prettyStatus(tr("Preparing to install selected packages..."))
{
    // Constructor - sets up the job and initial status message shown in the UI.
}

QString PackageSelectProcess::prettyName() const
{
    return tr("Installing selected packages");
}

QString PackageSelectProcess::prettyStatusMessage() const
{
    return m_prettyStatus;
}

void PackageSelectProcess::setConfigurationMap(const QVariantMap& configurationMap)
{
    m_configurationMap = configurationMap;
}

// Runs an apt command inside the chrooted target system with progress
// reporting and user-friendly status updates. Handles apt-get update,
// full-upgrade, install, remove, and autoremove.
Calamares::JobResult PackageSelectProcess::runAptCommand(const QString& command,
                                                         const QString& rootMountPoint,
                                                         double startProgress,
                                                         double endProgress,
                                                         bool verboseProgress)
{
    qDebug() << "Running apt command:" << command;
    QProcess aptProcess;
    aptProcess.setProgram("/usr/bin/chroot");
    QStringList args = {
        rootMountPoint,
        "/usr/bin/env",
        "DEBIAN_FRONTEND=noninteractive",
        "/bin/bash", "-c", command
    };
    aptProcess.setArguments(args);
    aptProcess.setProcessChannelMode(QProcess::MergedChannels);

    constexpr int MAX_LINES = 5000;
    double progressRange = endProgress - startProgress;
    double progressPerLine = progressRange / static_cast<double>(MAX_LINES);
    int lineCount = 0;

    QString commandHRPrefix;
    if (command.contains("install")) {
        commandHRPrefix = tr("Installing packages: ");
    } else if (command.contains("full-upgrade")) {
        commandHRPrefix = tr("Upgrading installed system: ");
    } else if (command.contains("remove")) {
        commandHRPrefix = tr("Cleaning up packages: ");
    } else if (command.contains("cdrom")) {
        commandHRPrefix = tr("cdrom: ");
    }

    QRegularExpression getRegex(R"(Get:\d+\s+[^ ]+\s+[^ ]+\s+(.+?)\s+\S+\s+(\S+)\s+\[(.*?)\])");

    connect(&aptProcess, &QProcess::readyReadStandardOutput, this,
        [this, &lineCount, progressPerLine, startProgress, endProgress, verboseProgress, commandHRPrefix, getRegex]() {
            QProcess *aptProcess = (QProcess *)(QObject::sender());
            if (aptProcess == NULL) return;
            while (aptProcess->canReadLine()) {
                QString line = QString::fromUtf8(aptProcess->readLine()).trimmed();
                if (line.isEmpty()) {
                    continue;
                }

                if (verboseProgress && !line.contains("Running in chroot, ignoring command") &&
                    !line.contains("Waiting until unit") && !line.contains("Stopping snap") &&
                    !line.contains("/dev/pts")) {

                    // Process "Get:" lines to show download information
                    if (line.startsWith("Get:")) {
                        QRegularExpressionMatch match = getRegex.match(line);
                        if (match.hasMatch()) {
                            QString packageName = match.captured(1);
                            QString packageVersion = match.captured(2);
                            QString packageSize = match.captured(3);
                            line = tr("Downloading %1 %2 (%3)").arg(packageName, packageVersion, packageSize);
                        }
                    }

                    m_prettyStatus = commandHRPrefix + line;
                    emit prettyStatusMessageChanged(m_prettyStatus);
                    qDebug() << m_prettyStatus;
                }

                lineCount++;
                double currentProgress = startProgress + (lineCount * progressPerLine);
                currentProgress = qBound(startProgress, currentProgress, endProgress);
                emit progress(currentProgress);
            }
        });

    aptProcess.start();
    if (!aptProcess.waitForStarted(5000)) {
        qWarning() << "Failed to start apt command:" << aptProcess.errorString();
        return Calamares::JobResult::error(tr("Apt command failed"),
                                           tr("Failed to start apt command: %1").arg(aptProcess.errorString()));
    }

    while (!aptProcess.waitForFinished(100)) {
        QCoreApplication::processEvents();
    }

    if (aptProcess.exitStatus() != QProcess::NormalExit || aptProcess.exitCode() != 0) {
        QString errorOutput = QString::fromUtf8(aptProcess.readAllStandardError()).trimmed();
        qWarning() << "Apt command error:" << errorOutput;
        return Calamares::JobResult::error(tr("Apt command failed"),
                                           tr("Failed to execute apt command: %1").arg(errorOutput));
    }

    emit progress(endProgress);
    m_prettyStatus = tr("Command executed successfully.");
    emit prettyStatusMessageChanged(m_prettyStatus);

    return Calamares::JobResult::ok();
}

// Handles installation of snap packages using the custom snapd-seed-glue
// tool that seeds the target system's snapd from the live environment.
Calamares::JobResult PackageSelectProcess::runSnapCommand(const QStringList& snapPackages,
                                                          const QString& rootMountPoint,
                                                          double startProgress,
                                                          double endProgress)
{
    const QString seedDirectory = QDir::cleanPath(rootMountPoint + "/var/lib/snapd/seed");
    QDir dir(seedDirectory);
    if (!dir.exists() && !dir.mkpath(".")) {
        return Calamares::JobResult::error(tr("Snap installation failed"),
                                           tr("Failed to create seed directory: %1").arg(seedDirectory));
    }

    QStringList snapCommandArgs = { "--seed", seedDirectory };
    snapCommandArgs += snapPackages;

    qDebug() << "Executing Snap Command:" << snapCommandArgs.join(" ");

    QProcess snapProcess(this);
    snapProcess.setProgram("/usr/bin/snapd-seed-glue");
    snapProcess.setArguments(snapCommandArgs);
    snapProcess.setProcessChannelMode(QProcess::MergedChannels);

    QString currentDescription;

    connect(&snapProcess, &QProcess::readyReadStandardOutput, this, 
        [&snapProcess, this, &currentDescription, startProgress, endProgress]( ) {
            while (snapProcess.canReadLine()) {
                QString line = QString::fromUtf8(snapProcess.readLine()).trimmed();
                if (line.isEmpty()) {
                    continue;
                }

                QStringList parts = line.split("\t");
                if (parts.size() != 2) {
                    qWarning() << "Unexpected output format from snap-seed-glue:" << line;
                    continue;
                }

                bool ok = false;
                double percentage = parts[0].toDouble(&ok);
                const QString& description = parts[1];

                if (!ok) {
                    qWarning() << "Failed to parse percentage from line:" << line;
                    continue;
                }

                if (description != currentDescription) {
                    m_prettyStatus = description;
                    emit prettyStatusMessageChanged(m_prettyStatus);
                    currentDescription = description;
                    qDebug() << description;
                }

                double scaledProgress = startProgress + (percentage / 100.0) * (endProgress - startProgress);
                emit progress(scaledProgress);
            }
        });

    m_prettyStatus = tr("Installing snap packages...");
    emit prettyStatusMessageChanged(m_prettyStatus);
    emit progress(startProgress);

    snapProcess.start();
    if (!snapProcess.waitForStarted()) {
        qWarning() << "Failed to start snap installation process:" << snapProcess.errorString();
        return Calamares::JobResult::error(tr("Snap installation failed"),
                                           tr("Failed to start snap installation process: %1").arg(snapProcess.errorString()));
    }

    while (!snapProcess.waitForFinished(100)) {
        QCoreApplication::processEvents();
    }

    if (snapProcess.exitStatus() != QProcess::NormalExit || snapProcess.exitCode() != 0) {
        QString errorOutput = QString::fromUtf8(snapProcess.readAllStandardError()).trimmed();
        qWarning() << "Snap installation error:" << errorOutput;
        return Calamares::JobResult::error(tr("Snap installation failed"),
                                           tr("Failed to install snap packages: %1").arg(errorOutput));
    }

    emit progress(endProgress);
    m_prettyStatus = tr("Snap packages installed successfully!");
    emit prettyStatusMessageChanged(m_prettyStatus);

    return Calamares::JobResult::ok();
}

// Helper to add/remove dpkg diversions for critical tools (dracut, update-initramfs, locale-gen)
// so they don't run during the live install and cause issues with the target system.
void PackageSelectProcess::divert(bool enable)
{
    for (auto it = dpkgDiversions.constBegin(); it != dpkgDiversions.constEnd(); ++it) {
        const QString& name = it.key();
        const QString& path = it.value();
        QString divertedPath = path + ".REAL";
        QString command;

        if (enable) {
            qDebug() << tr("Adding diversion for %1...").arg(name);
            command = QString("dpkg-divert --quiet --add --divert %1 --rename %2")
                .arg(divertedPath, path);
        } else {
            qDebug() << tr("Removing diversion for %1...").arg(name);
            QFile::remove(rootMountPoint + path);
            command = QString("dpkg-divert --quiet --remove --rename %1").arg(path);
        }

        // Set up the QProcess to run the command in chroot
        QProcess process;
        process.setProgram("/usr/bin/chroot");
        process.setArguments({ rootMountPoint, "/bin/bash", "-c", command });
        process.setProcessChannelMode(QProcess::MergedChannels);

        // Run the process
        process.start();
        if (!process.waitForFinished()) {
            qWarning() << "Process error:" << process.errorString();
            continue;
        }

        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            qWarning() << "Error handling diversion for" << name << ":" << process.readAll();
            continue;
        }

        if (!enable) { continue; }

        // Create the replacement script in chroot
        QString scriptContent = QString(
            "#!/bin/sh\n"
            "echo \"%1: diverted (will be called later)\" >&1\n"
            "exit 0\n"
        ).arg(name);

        QString scriptPath = rootMountPoint + path;
        QFile scriptFile(scriptPath);

        if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Error creating script for" << name << ":" << scriptFile.errorString();
            continue;
        }

        QTextStream out(&scriptFile);
        out << scriptContent;
        scriptFile.close();

        // Make the script executable
        QFile::setPermissions(scriptPath, QFile::permissions(scriptPath) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
    }
}

// Main execution entry point for the job. Orchestrates:
// - apt update
// - optional full-upgrade
// - package installation via checkpackage-backend
// - package removal
// - autoremove
// - snap seeding
// - offline installation hacks for ubuntu.sources
Calamares::JobResult PackageSelectProcess::exec()
{
    auto gs = Calamares::JobQueue::instance()->globalStorage();
    if (!gs || !gs->contains("installation_data")) {
        return Calamares::JobResult::error(tr("No installation data found."),
                                           tr("Installation data is missing from global storage."));
    }

    const QVariantMap installationData = gs->value("installation_data").toMap();
    const QString installationMode = installationData.value("installation_mode").toString();
    const bool hasInternet = gs->value("hasInternet").toBool();
    const bool downloadUpdates = (installationData.value("download_updates").toBool() && hasInternet);
    const QVariantList packagesToInstall = installationData.value("packages_to_install").toList();
    const QVariantList packagesToRemove = installationData.value("packages_to_remove").toList();
    const QVariantList presentSnaps = installationData.value("present_snaps").toList();

    // Handle default value for rootMountPoint
    rootMountPoint = "/";
    if (gs->contains("rootMountPoint")) {
        rootMountPoint = gs->value("rootMountPoint").toString();
    }

    static const QMap<QString, QVector<ProgressAllocation>> allocationMap = {
        { "minimal", { {0.0, 1.0} } },
        { "normal", { {0.0, 0.4}, {0.4, 1.0} } },
        { "full", { {0.0, 0.25}, {0.25, 1.0} } }
    };

    const QVector<ProgressAllocation> allocations = allocationMap.value(installationMode, { {0.0, 1.0} });
    const double aptRange = allocations[0].end - allocations[0].start;
    const double updateStart = allocations[0].start;
    const double updateEnd = updateStart + 0.1 * aptRange;

    // Temporarily copy ubuntu.sources elsewhere, if we do not have network
    // This is so we can update the apt cache safely
    // FIXME: there has to be a better, more native way to do this. It works
    // for now, but in the 26.10 cycle, we're probably going to move some of
    // these command-line apt calls to the libapt C library. LP: #2107287
    if (!hasInternet) {
        const QString ubuntu_sources_path = rootMountPoint + "/etc/apt/sources.list.d/ubuntu.sources";
        QFile* ubuntu_sources = new QFile(ubuntu_sources_path);
        // Just in case this module is used in a non-Ubuntu environment, make sure ubuntu.sources exists
        // TODO: make this configurable in the 26.10 cycle
        if (ubuntu_sources->exists()) {
            const QString backup_name = ubuntu_sources_path + ".bak";
            if (!ubuntu_sources->rename(backup_name)) {
                return Calamares::JobResult::error(tr("Internal Error"),
                                                   tr("Permission denied when moving ubuntu.sources to prepare for offline install"));
            }
        } else {
            return Calamares::JobResult::error(tr("Internal Error"),
                                               tr("/etc/apt/sources.list.d/ubuntu.sources not found in the target, are you a downstream?"));
        }
    }

    // Run apt update
    m_prettyStatus = tr("Updating apt cache");
    emit prettyStatusMessageChanged(m_prettyStatus);
    emit progress(updateStart);

    Calamares::JobResult updateResult = runAptCommand("apt-get update",
                                                      rootMountPoint,
                                                      updateStart,
                                                      updateEnd,
                                                      false);
    if (!updateResult) { // Using operator bool() to check for errors
        return std::move(updateResult); // Move to avoid copy
    }

    QStringList debPackages;
    for (const QVariant& var : packagesToInstall) {
        const QVariantMap pkg = var.toMap();
        if (!pkg.value("snap").toBool()) {
            debPackages << pkg.value("id").toString();
        }
    }

    // Add diversions for dracut, update-initramfs, and locale-gen
    /*
    dpkgDiversions = {
        {"dracut", "/usr/bin/dracut"},
        {"update-initramfs", "/usr/sbin/update-initramfs"},
        {"locale-gen", "/usr/sbin/locale-gen"}
    };
    divert(true);
    */

    double installStart;
    double installEnd;
    if (downloadUpdates) {
        const double upgradeStart = updateEnd;
        const double upgradeEnd = upgradeStart + 0.25 * aptRange;

        Calamares::JobResult upgradeResult = runAptCommand(
            "apt-get -y -o Dpkg::Options::='--force-confnew' full-upgrade",
            rootMountPoint,
            upgradeStart,
            upgradeEnd,
            true
        );
        if (!upgradeResult) { // Using operator bool() to check for errors
            return std::move(upgradeResult); // Move to avoid copy
        }

        installStart = upgradeEnd;
        installEnd = installStart + 0.25 * aptRange;
    }
    else {
        installStart = updateEnd;
        installEnd = installStart + 0.5 * aptRange;
        installEnd = qMin(installEnd, allocations[0].end);
    }

    qDebug() << "Progress range: installStart:" << installStart << "installEnd:" << installEnd;

    if (!debPackages.isEmpty()) {
        const QString checkpackage_path = "/usr/libexec/checkpackage-backend";
        const QString chroot_checkpackage_path = rootMountPoint + checkpackage_path;

        // Corresponding to the temporary hack in pkgselect adding calamares
        // and kdialog to the list, we only want those two included in the
        // final installation if we're actually in OEM mode. Otherwise, they
        // can be ignored, and are just clutter.
        // FIXME: When the OEM stack is rewritten in 26.10, this needs to be
        // removed.
        if (!QFile::exists("/etc/calamares/OEM_MODE_ACTIVATED")) {
            QStringList wip_list;
            for (auto debPackage : debPackages) {
                if (!debPackage.contains(QString("calamares")) &&
                    !debPackage.contains(QString("kdialog"))) {
                    wip_list << debPackage;
                }
            }
            debPackages = wip_list;
        }

        // checkpackage-backend needs to be explicitly copied to the chroot
        // and removed later for systems with stacked squashfses, or the
        // install command will fail. LP: #2104243
        QFile* cpbe = new QFile(chroot_checkpackage_path);
        bool did_copy_cpbe = false;
        if (!cpbe->exists()) {
            QFile* parent_cpbe = new QFile(checkpackage_path);
            if (!parent_cpbe->copy(chroot_checkpackage_path)) {
                return Calamares::JobResult::error(tr("Internal Error"),
                                                   tr("Permission denied when copying checkpackage-backend, are you running Calamares correctly?"));
            }
            did_copy_cpbe = true;
        }

        const QString packageList = debPackages.join(" ");
        const QString installCommand = QString("apt-get -y install $(/usr/libexec/checkpackage-backend %1);").arg(packageList);

        Calamares::JobResult installResult = runAptCommand(installCommand,
                                                           rootMountPoint,
                                                           installStart,
                                                           installEnd,
                                                           true);
        if (did_copy_cpbe) {
            if (!cpbe->remove()) qDebug() << "Warning: failed to clean up /usr/libexec/checkpackage-backend";
        }
        if (!installResult) return std::move(installResult);
    }
    else qDebug() << "No packages to install.";

    QStringList removeDebPackages;
    for (const QVariant& var : packagesToRemove) {
        removeDebPackages << var.toString();
    }

    // As part of the fix for LP: #2104343, we need to ensure that, if
    // we are currently in OEM mode, Calamares and friends remain
    // installed. During stage two, we clean it up.
    // FIXME: When the OEM stack is rewritten in 26.10, this needs to be
    // rewritten.
    if (QFile::exists("/etc/calamares/OEM_MODE_ACTIVATED")) {
        QStringList wip_list;
        for (auto removeDebPackage : removeDebPackages) {
            if (!removeDebPackage.contains(QString("calamares"))) wip_list << removeDebPackage;
        }
        removeDebPackages = wip_list;
    }

    const double removeStart = installEnd;
    const double removeEnd = removeStart + 0.2 * aptRange;

    if (!removeDebPackages.isEmpty()) {
        const QString removeCommand = QString("apt-get -y --purge remove $(/usr/libexec/checkpackage-backend %1);")
                                         .arg(removeDebPackages.join(" "));
        Calamares::JobResult removeResult = runAptCommand(removeCommand,
                                                          rootMountPoint,
                                                          removeStart,
                                                          removeEnd,
                                                          true);
        if (!removeResult) return std::move(removeResult);
    }

    const double autoremoveStart = removeEnd;
    const double autoremoveEnd = autoremoveStart + 0.2 * aptRange;

    Calamares::JobResult autoremoveResult = runAptCommand("apt-get -y autoremove",
                                                           rootMountPoint,
                                                           autoremoveStart,
                                                           autoremoveEnd,
                                                           true);

    // Disable diversions
    //divert(false);

    // Move ubuntu.sources back, and clean up the cdrom file
    // FIXME: there has to be a better, more native way to do this. It works
    // for now, but in the 26.10 cycle, we're probably going to move some of
    // these command-line apt calls to the libapt C library. LP: #2107287
    try {
        if (!hasInternet) {
            const QString ubuntu_sources_path = rootMountPoint + "/etc/apt/sources.list.d/ubuntu.sources";
            const QString backup_name = ubuntu_sources_path + ".bak";
            QFile* ubuntu_sources = new QFile(ubuntu_sources_path);
            QFile* ubuntu_sources_bak = new QFile(backup_name);
            // Just in case this module is used in a non-Ubuntu environment, make sure ubuntu.sources exists
            // TODO: make this configurable in the 26.10 cycle
            if (ubuntu_sources->exists()) {
                if (!ubuntu_sources->remove()) {
                    return Calamares::JobResult::error(tr("Internal Error"),
                                                       tr("/etc/apt/sources.list.d/ubuntu.sources already exists and it won't budge - this is a rare edge case, please report!"));
                }
            }
            if (!ubuntu_sources_bak->rename(ubuntu_sources_path)) {
                return Calamares::JobResult::error(tr("Internal Error"),
                                                   tr("Permission denied when moving ubuntu.sources back after offline install"));
            }
        }
    } catch (const std::exception &exc) {
        qDebug() << exc.what();
    } catch (...) {
        qDebug() << "Caught unknown error";
    }

    // Handle snap packages
    if (installationMode != "minimal") {
        QStringList snapPackages;
        QStringList presentSnapsList;
        // Convert QVariantList to QStringList
        for (const QVariant& var : presentSnaps) {
            presentSnapsList << var.toString();
        }

        for (const QVariant& var : packagesToInstall) {
            const QVariantMap pkg = var.toMap();
            if (pkg.value("snap").toBool()) {
                snapPackages << pkg.value("id").toString();
            }
        }

        QStringList finalSnapPackages;

        if (!snapPackages.isEmpty() && !presentSnapsList.isEmpty()) {
            finalSnapPackages = presentSnapsList + snapPackages;
        }
        else if (!snapPackages.isEmpty()) {
            finalSnapPackages = snapPackages;
        }
        else if (!presentSnapsList.isEmpty() && downloadUpdates) {
            finalSnapPackages = presentSnapsList;
        }

        if (!finalSnapPackages.isEmpty()) {
            double snapStart = allocations.size() > 1 ? allocations[1].start : allocations[0].end;
            double snapEnd = allocations.size() > 1 ? allocations[1].end : allocations[0].end;

            Calamares::JobResult snapResult = runSnapCommand(finalSnapPackages,
                                                            rootMountPoint,
                                                            snapStart,
                                                            snapEnd);
            if (!snapResult) { // Using operator bool() to check for errors
                return std::move(snapResult); // Move to avoid copy
            }
        }
    }

    emit progress(1.0);
    m_prettyStatus = tr("All selected packages installed successfully.");
    emit prettyStatusMessageChanged(m_prettyStatus);

    return Calamares::JobResult::ok();
}
