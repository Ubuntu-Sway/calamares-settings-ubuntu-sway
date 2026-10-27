#ifndef PACKAGESELECTPROCESS_H
#define PACKAGESELECTPROCESS_H

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include "CppJob.h"
#include "utils/PluginFactory.h"
#include "DllMacro.h"

class QProcess;

// Main Calamares C++ job class for package selection and installation.
// This job is responsible for the bulk of the "install selected packages"
// phase in the Ubuntu Calamares-based installer.
class PLUGINDLLEXPORT PackageSelectProcess : public Calamares::CppJob
{
    Q_OBJECT

public:
    explicit PackageSelectProcess(QObject* parent = nullptr);
    ~PackageSelectProcess() override = default;

    QString prettyName() const override;
    QString prettyStatusMessage() const override;

    Calamares::JobResult exec() override;

    void setConfigurationMap(const QVariantMap& configurationMap) override;

signals:
    void prettyStatusMessageChanged(const QString&);

private:
    struct ProgressAllocation
    {
        double start;
        double end;
    };

    // Runs apt commands (update, upgrade, install, remove) inside chroot
    // with progress feedback.
    Calamares::JobResult runAptCommand(const QString& command,
                                       const QString& rootMountPoint,
                                       double startProgress,
                                       double endProgress,
                                       bool verboseProgress);

    // Installs snaps using snapd-seed-glue.
    Calamares::JobResult runSnapCommand(const QStringList& snapPackages,
                                        const QString& rootMountPoint,
                                        double startProgress,
                                        double endProgress);

    // Manages dpkg diversions for critical postinst scripts.
    void divert(bool enable);

    QMap<QString, QString> dpkgDiversions;
    QString rootMountPoint;

    QVariantMap m_configurationMap;
    QString m_prettyStatus;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION(PackageSelectProcessFactory)

#endif // PACKAGESELECTPROCESS_H
