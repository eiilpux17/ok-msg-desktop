/*
 * Copyright (c) 2022 船山信息 chuanshaninfo.com
 * The project is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PubL v2. You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include "install_osx.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

#include <QProcess>
#include <QStandardPaths>

#include <unistd.h>
#include <QMessageBox>

void osx::moveToAppFolder() {
    if (qApp->applicationDirPath() != "/Applications/qtox.app/Contents/MacOS") {
        qDebug() << "OS X: Not in Applications folder";

        QMessageBox AskInstall;
        AskInstall.setIcon(QMessageBox::Question);
        AskInstall.setWindowModality(Qt::ApplicationModal);
        AskInstall.setText("Move to Applications folder?");
        AskInstall.setInformativeText(
                "I can move myself to the Applications folder, keeping your "
                "downloads folder less cluttered.\r\n");
        AskInstall.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        AskInstall.setDefaultButton(QMessageBox::Yes);

        int AskInstallAttempt = AskInstall.exec();  // Actually ask the user

        if (AskInstallAttempt == QMessageBox::Yes) {
            QProcess* sudoprocess = new QProcess;
            QProcess* qtoxprocess = new QProcess;

            QString bindir = qApp->applicationDirPath();
            QString appdir = bindir;
            appdir.chop(15);

            QString appdir_noqtox = appdir;
            appdir_noqtox.chop(8);

            if ((appdir_noqtox + "qtox.app") != appdir)  // quick safety check
            {
                qDebug() << "OS X: Attmepted to delete non OkMsg directory!";
                exit(EXIT_UPDATE_MACX_FAIL);
            }

            QDir old_app(appdir);

            const QString sudoProgram = bindir + "/qtox_sudo";
            const QStringList sudoArguments = {"rsync", "-avzhpltK", appdir, "/Applications"};
            sudoprocess->start(sudoProgram,
                               sudoArguments);  // Where the magic actually happens, safety checks ^
            sudoprocess->waitForFinished();

            if (old_app.removeRecursively())  // We've just deleted the running program
                qDebug() << "OS X: Cleaned up old directory";
            else
                qDebug() << "OS X: This should never happen, the directory failed to delete";

            if (fork() != 0)  // Forking is required otherwise it won't actually cleanly launch
                exit(EXIT_UPDATE_MACX);

            const QString qtoxProgram = "open";
            const QStringList qtoxArguments = {"/Applications/qtox.app"};
            qtoxprocess->start(qtoxProgram, qtoxArguments);

            exit(0);  // Actually kills it
        }
    }
}
// migrateProfiles() is compatabilty code that can be removed down the line when the time seems
// right.
void osx::migrateProfiles() {
    QString oldPath = QDir::cleanPath(
            QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + QDir::separator() +
            "Library" + QDir::separator() + "Preferences" + QDir::separator() + "tox");
    QFileInfo checkDir(oldPath);

    QString newPath = QDir::cleanPath(
            QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + QDir::separator() +
            "Library" + QDir::separator() + "Application Support" + QDir::separator() + "Tox");
    QDir dir;

    if (!checkDir.exists() || !checkDir.isDir()) {
        qDebug() << "OS X: Old settings directory not detected";
        return;
    }

    qDebug() << "OS X: Old settings directory detected migrating to default";
    if (!dir.rename(oldPath, newPath)) {
        qDebug() << "OS X: Profile migration failed. ~/Library/Application Support/Tox already "
                    "exists. Using alternate migration method.";
        QString OSXMigrater = "../Resources/OSX-Migrater.sh";
        QProcess::execute(OSXMigrater, {});
        QMessageBox MigrateProfile;
        MigrateProfile.setIcon(QMessageBox::Information);
        MigrateProfile.setWindowModality(Qt::ApplicationModal);
        MigrateProfile.setText("Alternate profile migration method used.");
        MigrateProfile.setInformativeText(
                "It has been detected that your profiles \nwhere migrated to the new settings "
                "directory; \nusing the alternate migration method. \n\nA backup can be found in "
                "your: "
                "\n/Users/[USER]/.Tox-Backup[DATE-TIME] \n\nJust in case. \r\n");
        MigrateProfile.exec();
    }
}
// End migrateProfiles() compatibility code
