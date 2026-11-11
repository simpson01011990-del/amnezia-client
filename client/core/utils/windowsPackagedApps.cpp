#include "windowsPackagedApps.h"

#include <windows.h>
#include <appmodel.h>

#include <QDir>
#include <QFile>
#include <QXmlStreamReader>

#include <string>
#include <vector>

namespace
{
    const QString shellAppsFolderPrefix = QStringLiteral("shell:AppsFolder\\");

    QStringList installDirectories(const QString &packageFamilyName)
    {
        const std::wstring family = packageFamilyName.toStdWString();

        UINT32 packageCount = 0;
        UINT32 bufferLength = 0;
        if (GetPackagesByPackageFamily(family.c_str(), &packageCount, nullptr, &bufferLength, nullptr) != ERROR_INSUFFICIENT_BUFFER) {
            return {};
        }

        std::vector<PWSTR> packageFullNames(packageCount);
        std::vector<wchar_t> buffer(bufferLength);
        if (GetPackagesByPackageFamily(family.c_str(), &packageCount, packageFullNames.data(), &bufferLength, buffer.data())
            != ERROR_SUCCESS) {
            return {};
        }

        QStringList directories;
        for (UINT32 i = 0; i < packageCount; ++i) {
            UINT32 pathLength = 0;
            if (GetPackagePathByFullName(packageFullNames[i], &pathLength, nullptr) != ERROR_INSUFFICIENT_BUFFER) {
                continue;
            }

            std::vector<wchar_t> path(pathLength);
            if (GetPackagePathByFullName(packageFullNames[i], &pathLength, path.data()) == ERROR_SUCCESS) {
                directories.append(QString::fromWCharArray(path.data()));
            }
        }
        return directories;
    }

    QStringList manifestExecutables(const QString &installDirectory, const QString &applicationId)
    {
        const QDir directory(installDirectory);
        QFile manifest(directory.filePath(QStringLiteral("AppxManifest.xml")));
        if (!manifest.open(QIODevice::ReadOnly)) {
            return {};
        }

        QStringList executables;
        QXmlStreamReader xml(&manifest);
        while (!xml.atEnd()) {
            if (xml.readNext() != QXmlStreamReader::StartElement || xml.name() != QLatin1String("Application")) {
                continue;
            }

            const auto attributes = xml.attributes();
            const QString executable = attributes.value(QLatin1String("Executable")).toString();
            const bool isRequestedApplication =
                    applicationId.isEmpty() || attributes.value(QLatin1String("Id")).compare(applicationId, Qt::CaseInsensitive) == 0;
            if (isRequestedApplication && !executable.isEmpty()) {
                executables.append(QDir::toNativeSeparators(directory.filePath(executable)));
            }
        }
        return executables;
    }
}

QStringList WindowsPackagedApps::executablePaths(const QString &appIdentifier)
{
    QString identifier = appIdentifier.trimmed();
    if (identifier.startsWith(shellAppsFolderPrefix, Qt::CaseInsensitive)) {
        identifier.remove(0, shellAppsFolderPrefix.size());
    }

    const QString packageFamilyName = identifier.section(QLatin1Char('!'), 0, 0);
    const QString applicationId = identifier.section(QLatin1Char('!'), 1);

    QStringList executables;
    for (const QString &installDirectory : installDirectories(packageFamilyName)) {
        executables.append(manifestExecutables(installDirectory, applicationId));
    }
    executables.removeDuplicates();
    return executables;
}
