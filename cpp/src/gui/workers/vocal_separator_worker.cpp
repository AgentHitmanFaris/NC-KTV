#include "vocal_separator_worker.h"
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>

namespace ncktv {

VocalSeparatorWorker::VocalSeparatorWorker(QObject* parent)
    : QObject(parent)
{
}

void VocalSeparatorWorker::startSeparation(const QString& audioPath, 
                                           const QString& modelName, 
                                           const QString& outputDir) 
{
    QString exePath = "D:/Document/NC-KTV/python_embed/Scripts/audio-separator.exe";
    if (!QFile::exists(exePath)) {
        emit error("audio-separator executable not found at " + exePath);
        return;
    }

    QDir().mkpath(outputDir);

    QProcess* proc = new QProcess(this);
    proc->setWorkingDirectory(QDir::currentPath());

    // Inject the Python Scripts directory into the PATH for audio-separator to find ffmpeg
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString currentPath = env.value("PATH");
    QString scriptsPath = QFileInfo(exePath).absolutePath();
    env.insert("PATH", scriptsPath + ";" + currentPath);
    proc->setProcessEnvironment(env);

    connect(proc, &QProcess::readyReadStandardError, this, [this, proc]() {
        QString output = QString::fromUtf8(proc->readAllStandardError());
        // Simple progress parsing from audio-separator logs
        // Usually shows percentage or step info
        if (output.contains("%")) {
            static QRegularExpression re("(\\d+)%");
            auto match = re.match(output);
            if (match.hasMatch()) {
                emit progress(match.captured(1).toInt(), "Separating vocals...");
            }
        }
        qDebug() << "UVR Log:" << output;
    });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, [this, proc, audioPath, outputDir, modelName](int exitCode) {
        if (exitCode == 0) {
            // Find output files
            QString instrumentalPath;
            QString vocalsPath;
            
            QDir outDir(outputDir);
            QString baseName = QFileInfo(audioPath).completeBaseName();
            
            // audio-separator usually names files as: [BaseName]_(Instrumental)_[Model].wav
            for (const auto& entry : outDir.entryInfoList(QDir::Files)) {
                if (entry.fileName().contains(baseName)) {
                    if (entry.fileName().contains("Instrumental", Qt::CaseInsensitive))
                        instrumentalPath = entry.absoluteFilePath();
                    else if (entry.fileName().contains("Vocals", Qt::CaseInsensitive))
                        vocalsPath = entry.absoluteFilePath();
                }
            }

            if (instrumentalPath.isEmpty() || vocalsPath.isEmpty()) {
                emit error("Separation seemed to finish but output files were not found.");
            } else {
                emit separationComplete(instrumentalPath, vocalsPath);
            }
        } else {
            emit error(QString("Vocal separation failed with exit code %1").arg(exitCode));
        }
        proc->deleteLater();
    });

    QStringList args = {
        audioPath,
        "--model_filename", modelName,
        "--output_dir", outputDir,
        "--output_format", "WAV"
    };

    emit progress(5, "Initializing UVR model...");
    proc->start(exePath, args);
}

} // namespace ncktv
