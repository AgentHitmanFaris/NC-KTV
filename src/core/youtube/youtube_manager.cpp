#include "youtube_manager.h"
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QFileInfo>
#include <QStandardPaths>
#include <iostream>
#include <nlohmann/json.hpp>

namespace ncktv {

static QString findFfmpegPath() {
    static const QString cachedPath = []() {
        // 1. Try PATH
        {
            QProcess proc;
            proc.setProgram("ffmpeg");
            proc.setArguments({"-version"});
            proc.start();
            if (proc.waitForStarted(1000)) {
                if (proc.waitForFinished(2000)) {
                    if (proc.exitCode() == 0) {
                        return QString("ffmpeg");
                    }
                } else {
                    proc.kill();
                    proc.waitForFinished(500);
                }
            }
        }

        // 2. Try application relative path (e.g. downloaded by download_ffmpeg.py)
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList relativePaths = {
            appDir + "/ffmpeg.exe",
            appDir + "/ffmpeg/bin/ffmpeg.exe",
            appDir + "/../ffmpeg/bin/ffmpeg.exe",
            appDir + "/../../ffmpeg/bin/ffmpeg.exe",
            appDir + "/../../../ffmpeg/bin/ffmpeg.exe",
            appDir + "/../../../../ffmpeg/bin/ffmpeg.exe",
            appDir + "/../../../../../ffmpeg/bin/ffmpeg.exe",
            appDir + "/build/ffmpeg.exe",
            appDir + "/../build/ffmpeg.exe",
            appDir + "/../../build/ffmpeg.exe",
            appDir + "/../../../build/ffmpeg.exe",
            appDir + "/../../../../build/ffmpeg.exe",
            appDir + "/../../../../../build/ffmpeg.exe"
        };
        for (const QString& path : relativePaths) {
            if (QFile::exists(path)) {
                return QDir::cleanPath(path);
            }
        }

        // 3. Try common system paths
        QStringList commonPaths = {
            "D:\\Program Files\\FFmpeg\\Providers\\org.buanzo.ffmpeg8.1\\bin\\ffmpeg.exe",
            "C:\\Program Files\\FFmpeg\\bin\\ffmpeg.exe",
            "D:\\Program Files\\FFmpeg\\bin\\ffmpeg.exe",
            "C:\\Program Files (x86)\\FFmpeg\\bin\\ffmpeg.exe",
            "C:\\msys64\\usr\\bin\\ffmpeg.exe"
        };

        for (const QString& path : commonPaths) {
            if (QFile::exists(path)) {
                return path;
            }
        }

        return QString("");
    }();
    return cachedPath;
}

YoutubeManager::YoutubeManager(QObject* parent)
    : QObject(parent) {
}

YoutubeManager::~YoutubeManager() {
    if (m_searchProcess) {
        m_searchProcess->kill();
        m_searchProcess->waitForFinished(500);
    }
    for (QProcess* proc : m_downloadProcesses.values()) {
        proc->kill();
        proc->waitForFinished(500);
    }
}

bool YoutubeManager::findYtDlpCommand(QString& cmd, QStringList& fallbackArgs) const {
    if (m_hasCachedCmd) {
        cmd = m_cachedCmd;
        fallbackArgs = m_cachedFallbackArgs;
        return !m_cachedCmd.isEmpty();
    }

    // 1. Try "yt-dlp" in system PATH
    {
        QProcess proc;
        proc.setProgram("yt-dlp");
        proc.setArguments({"--version"});
        proc.start();
        if (proc.waitForStarted(1000)) {
            if (proc.waitForFinished(2000)) {
                if (proc.exitCode() == 0) {
                    m_cachedCmd = "yt-dlp";
                    m_cachedFallbackArgs.clear();
                    m_hasCachedCmd = true;
                    cmd = m_cachedCmd;
                    fallbackArgs = m_cachedFallbackArgs;
                    return true;
                }
            } else {
                proc.kill();
                proc.waitForFinished(500);
            }
        }
    }

    // 2. Try "python -m yt_dlp"
    {
        QProcess proc;
        proc.setProgram("python");
        proc.setArguments({"-m", "yt_dlp", "--version"});
        proc.start();
        if (proc.waitForStarted(1000)) {
            if (proc.waitForFinished(2000)) {
                if (proc.exitCode() == 0) {
                    m_cachedCmd = "python";
                    m_cachedFallbackArgs = {"-m", "yt_dlp"};
                    m_hasCachedCmd = true;
                    cmd = m_cachedCmd;
                    fallbackArgs = m_cachedFallbackArgs;
                    return true;
                }
            } else {
                proc.kill();
                proc.waitForFinished(500);
            }
        }
    }

    // 2.5. Try user's specific embedded python interpreter path
    {
        QProcess proc;
        proc.setProgram("D:\\Program Files\\pythonembededglobal\\python.exe");
        proc.setArguments({"-m", "yt_dlp", "--version"});
        proc.start();
        if (proc.waitForStarted(1000)) {
            if (proc.waitForFinished(2000)) {
                if (proc.exitCode() == 0) {
                    m_cachedCmd = "D:\\Program Files\\pythonembededglobal\\python.exe";
                    m_cachedFallbackArgs = {"-m", "yt_dlp"};
                    m_hasCachedCmd = true;
                    cmd = m_cachedCmd;
                    fallbackArgs = m_cachedFallbackArgs;
                    return true;
                }
            } else {
                proc.kill();
                proc.waitForFinished(500);
            }
        }
    }

    // 3. Try standard Windows paths or common Python directories
    QStringList commonPaths = {
        "D:\\ProgramData\\chocolatey\\bin\\yt-dlp.exe",
        "C:\\ProgramData\\chocolatey\\bin\\yt-dlp.exe",
        "C:\\msys64\\usr\\bin\\yt-dlp",
        QDir::homePath() + "\\AppData\\Local\\Programs\\Python\\Python311\\Scripts\\yt-dlp.exe",
        QDir::homePath() + "\\AppData\\Local\\Programs\\Python\\Python312\\Scripts\\yt-dlp.exe"
    };

    for (const QString& path : commonPaths) {
        if (QFile::exists(path)) {
            m_cachedCmd = path;
            m_cachedFallbackArgs.clear();
            m_hasCachedCmd = true;
            cmd = m_cachedCmd;
            fallbackArgs = m_cachedFallbackArgs;
            return true;
        }
    }

    // Cache the failure too, so we don't keep blocking the UI thread with failed checks
    m_cachedCmd = "";
    m_cachedFallbackArgs.clear();
    m_hasCachedCmd = true;
    return false;
}

void YoutubeManager::search(const QString& query) {
    if (m_isSearching) {
        emit searchFailed("A search is already in progress.");
        return;
    }

    if (query.trimmed().isEmpty()) {
        emit searchFailed("Search query cannot be empty.");
        return;
    }

    m_lastQuery = query;
    m_currentPage = 1;
    
    if (m_hasMoreResults) {
        m_hasMoreResults = false;
        emit hasMoreResultsChanged();
    }

    runSearch(query, 1);
}

void YoutubeManager::searchMore() {
    if (m_isSearching) {
        emit searchFailed("A search is already in progress.");
        return;
    }

    if (m_lastQuery.trimmed().isEmpty()) {
        emit searchFailed("No active search query.");
        return;
    }

    if (!m_hasMoreResults) {
        return;
    }

    runSearch(m_lastQuery, m_currentPage + 1);
}

void YoutubeManager::runSearch(const QString& query, int page) {
    QString cmd;
    QStringList args;
    if (!findYtDlpCommand(cmd, args)) {
        m_lastError = "yt-dlp dependency not found on your system PATH.\nPlease install python & run 'pip install yt-dlp'.";
        emit errorOccurred(m_lastError);
        emit searchFailed(m_lastError);
        return;
    }

    m_isSearching = true;
    m_lastError.clear();
    m_searchOutputBuffer.clear();
    emit isSearchingChanged();

    m_currentPage = page;

    int playlistStart = (page - 1) * 10 + 1;
    int playlistEnd = page * 10;
    QString searchExpr = QString("ytsearch%1:%2").arg(playlistEnd).arg(query);
    args.append({
        searchExpr,
        "--playlist-start", QString::number(playlistStart),
        "--playlist-end", QString::number(playlistEnd),
        "--dump-json",
        "--flat-playlist",
        "--no-playlist"
    });

    m_searchProcess = new QProcess(this);
    m_searchProcess->setProgram(cmd);
    m_searchProcess->setArguments(args);

    connect(m_searchProcess, &QProcess::readyReadStandardOutput, this, &YoutubeManager::onSearchReadyRead);
    connect(m_searchProcess, &QProcess::finished, this, &YoutubeManager::onSearchFinished);

    std::cout << "[YT SEARCH] Spawning process (Page " << page << "): " << cmd.toStdString() << " " << args.join(" ").toStdString() << "\n";
    m_searchProcess->start();
}

void YoutubeManager::onSearchReadyRead() {
    if (m_searchProcess) {
        m_searchOutputBuffer.append(m_searchProcess->readAllStandardOutput());
    }
}

void YoutubeManager::onSearchFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitStatus);
    m_isSearching = false;
    emit isSearchingChanged();

    if (exitCode != 0) {
        QString stderrOutput = m_searchProcess ? QString::fromUtf8(m_searchProcess->readAllStandardError()).trimmed() : "";
        m_lastError = "Search process failed. Check network connection.\n" + stderrOutput;
        
        // If pagination failed, restore previous page state
        if (m_currentPage > 1) {
            m_currentPage--;
        }
        
        emit errorOccurred(m_lastError);
        emit searchFailed(m_lastError);
        if (m_searchProcess) {
            m_searchProcess->deleteLater();
            m_searchProcess = nullptr;
        }
        return;
    }

    // Parse flat-playlist line-by-line JSON output
    QVariantList results;
    QString rawOutput = QString::fromUtf8(m_searchOutputBuffer);
    QStringList lines = rawOutput.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        try {
            nlohmann::json j = nlohmann::json::parse(line.toStdString());
            
            QVariantMap trackMap;
            trackMap["id"] = QString::fromStdString(j.value("id", ""));
            trackMap["title"] = QString::fromStdString(j.value("title", ""));
            trackMap["channel"] = QString::fromStdString(j.value("uploader", ""));
            
            double durationSecs = j.value("duration", 0.0);
            trackMap["duration"] = durationSecs;
            
            // Format duration as mm:ss
            int mins = static_cast<int>(durationSecs) / 60;
            int secs = static_cast<int>(durationSecs) % 60;
            trackMap["durationStr"] = QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0'));
            
            // Construct a robust, high-resolution direct thumbnail link
            trackMap["thumbnail"] = QString("https://i.ytimg.com/vi/%1/hqdefault.jpg").arg(trackMap["id"].toString());
            
            results.append(trackMap);
        } catch (...) {
            // Ignore malformed JSON lines
        }
    }

    std::cout << "[YT SEARCH] Completed Page " << m_currentPage << ". Found " << results.size() << " tracks.\n";
    
    bool oldHasMore = m_hasMoreResults;
    // If we returned less than 10 results, we've hit the end of the search results
    m_hasMoreResults = (results.size() >= 10);
    if (m_hasMoreResults != oldHasMore) {
        emit hasMoreResultsChanged();
    }

    if (m_currentPage == 1) {
        emit searchCompleted(results);
    } else {
        emit moreResultsLoaded(results);
    }

    if (m_searchProcess) {
        m_searchProcess->deleteLater();
        m_searchProcess = nullptr;
    }
}

void YoutubeManager::download(const QString& videoId, bool audioOnly, const QString& saveDir, const QString& title) {
    if (videoId.isEmpty()) return;

    if (m_downloadProcesses.contains(videoId)) {
        emit downloadFailed(videoId, "This video download is already in progress.");
        return;
    }

    QString cmd;
    QStringList args;
    if (!findYtDlpCommand(cmd, args)) {
        m_lastError = "yt-dlp dependency not found on your system PATH.";
        emit errorOccurred(m_lastError);
        emit downloadFailed(videoId, m_lastError);
        return;
    }

    QDir dir(saveDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString safeTitle = title.trimmed();
    if (safeTitle.isEmpty()) {
        safeTitle = videoId;
    } else {
        // Sanitize title for filename: replace \ / : * ? " < > | with _
        safeTitle.replace(QRegularExpression(R"([\\/:*?"<>|])"), "_");
    }

    QString finalOutFile;
    if (audioOnly) {
        finalOutFile = dir.absoluteFilePath(safeTitle + ".wav");
        args.append({
            "-x",
            "--audio-format", "wav",
            "--audio-quality", "0",
            "-o", dir.absoluteFilePath(safeTitle + ".%(ext)s")
        });
    } else {
        finalOutFile = dir.absoluteFilePath(safeTitle + ".mp4");
        args.append({
            "-f", "bestvideo[vcodec^=avc1][height<=1080]+bestaudio[acodec^=mp4a]/best[vcodec^=avc1][height<=1080]/best[ext=mp4]/best",
            "-o", dir.absoluteFilePath(safeTitle + ".%(ext)s")
        });
    }

    QString ffmpeg = findFfmpegPath();
    if (!ffmpeg.isEmpty()) {
        args.append({"--ffmpeg-location", ffmpeg});
    }

    // Check if a local cookies.txt file exists to bypass DPAPI decrypt failures
    QString localCookiesFile;
    QStringList cookiesSearchPaths = {
        QCoreApplication::applicationDirPath() + "/cookies.txt",
        QCoreApplication::applicationDirPath() + "/../cookies.txt",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/NC-KTV Downloads/cookies.txt",
        QDir::currentPath() + "/cookies.txt"
    };

    for (const QString& path : cookiesSearchPaths) {
        if (QFile::exists(path)) {
            localCookiesFile = QDir::toNativeSeparators(path);
            break;
        }
    }

    if (!localCookiesFile.isEmpty()) {
        args.append({"--cookies", localCookiesFile});
        std::cout << "[YT DOWNLOAD] Using manual cookie file: " << localCookiesFile.toStdString() << "\n";
    } else {
        // Append cookies options based on retry count
        int retryCount = m_downloadRetryCount.value(videoId, 0);
        if (retryCount == 1) {
            args.append({"--cookies-from-browser", "edge"});
            std::cout << "[YT DOWNLOAD] Appending --cookies-from-browser edge (Retry level 1)\n";
        } else if (retryCount == 2) {
            args.append({"--cookies-from-browser", "chrome"});
            std::cout << "[YT DOWNLOAD] Appending --cookies-from-browser chrome (Retry level 2)\n";
        } else if (retryCount == 3) {
            args.append({"--cookies-from-browser", "firefox"});
            std::cout << "[YT DOWNLOAD] Appending --cookies-from-browser firefox (Retry level 3)\n";
        }
    }

    args.append("https://www.youtube.com/watch?v=" + videoId);

    QProcess* proc = new QProcess(this);
    proc->setProgram(cmd);
    proc->setArguments(args);

    if (!ffmpeg.isEmpty() && ffmpeg != "ffmpeg") {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString ffmpegDir = QFileInfo(ffmpeg).absolutePath();
        QString pathVal = env.value("PATH");
        if (!pathVal.isEmpty()) {
            env.insert("PATH", ffmpegDir + ";" + pathVal);
        } else {
            env.insert("PATH", ffmpegDir);
        }
        proc->setProcessEnvironment(env);
    }

    m_downloadProcesses[videoId] = proc;
    m_downloadOutputPaths[videoId] = finalOutFile;
    m_downloadAudioOnly[videoId] = audioOnly;
    m_downloadSaveDirs[videoId] = saveDir;
    m_downloadTitles[videoId] = title;

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, videoId]() {
        onDownloadReadyRead(videoId);
    });

    connect(proc, &QProcess::finished, this, [this, videoId](int exitCode, QProcess::ExitStatus status) {
        onDownloadFinished(videoId, exitCode, status);
    });

    std::cout << "[YT DOWNLOAD] Spawning: " << cmd.toStdString() << " " << args.join(" ").toStdString() << "\n";
    proc->start();
    emit downloadProgress(videoId, 0.0);
}

void YoutubeManager::cancelDownload(const QString& videoId) {
    if (m_downloadProcesses.contains(videoId)) {
        std::cout << "[YT DOWNLOAD] Cancelling: " << videoId.toStdString() << "\n";
        QProcess* proc = m_downloadProcesses[videoId];
        if (proc) {
            proc->kill();
        }
        // Handler onDownloadFinished will trigger cleanup and emit downloadFailed.
    }
}

void YoutubeManager::onDownloadReadyRead(const QString& videoId) {
    QProcess* proc = m_downloadProcesses.value(videoId, nullptr);
    if (!proc) return;

    QString output = QString::fromUtf8(proc->readAllStandardOutput());
    // Parse yt-dlp download progress format: "[download]  34.5% of 12.34MiB at ..."
    static QRegularExpression rx(R"(\[\s*download\s*\]\s*([\d\.]+)\s*%)");
    
    double lastProgress = -1.0;
    auto matches = rx.globalMatch(output);
    while (matches.hasNext()) {
        auto match = matches.next();
        double pct = match.captured(1).toDouble();
        lastProgress = pct / 100.0;
    }

    if (lastProgress >= 0.0) {
        emit downloadProgress(videoId, lastProgress);
    }
}

void YoutubeManager::onDownloadFinished(const QString& videoId, int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitStatus);

    QProcess* proc = m_downloadProcesses.take(videoId);
    QString outPath = m_downloadOutputPaths.take(videoId);
    bool audioOnly = m_downloadAudioOnly.take(videoId);
    QString saveDir = m_downloadSaveDirs.take(videoId);
    QString title = m_downloadTitles.take(videoId);

    if (exitCode != 0) {
        QString stdErr;
        if (proc) {
            stdErr = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        }

        if (proc) {
            proc->deleteLater();
        }

        // If file exists even with non-zero exit code (e.g. merger warning), let's see if we can still count it
        if (QFile::exists(outPath)) {
            std::cout << "[YT DOWNLOAD] Finished with exit code " << exitCode << " but output file exists, assuming success.\n";
            m_downloadRetryCount.remove(videoId);
            emit downloadProgress(videoId, 1.0);
            emit downloadCompleted(videoId, outPath, audioOnly);
            return;
        }

        // Check if error is related to bot check/sign in
        int currentRetry = m_downloadRetryCount.value(videoId, 0);
        bool isBotCheck = stdErr.contains("bot", Qt::CaseInsensitive) ||
                          stdErr.contains("sign in", Qt::CaseInsensitive) ||
                          stdErr.contains("confirm you", Qt::CaseInsensitive) ||
                          stdErr.contains("cookies", Qt::CaseInsensitive);

        if (isBotCheck && currentRetry < 3) {
            m_downloadRetryCount[videoId] = currentRetry + 1;
            std::cout << "[YT DOWNLOAD] Bot detection detected. Retrying with browser cookies. Retry level: " << (currentRetry + 1) << "\n";
            // Re-invoke download with new retry level
            download(videoId, audioOnly, saveDir, title);
            return;
        }

        m_downloadRetryCount.remove(videoId);
        QString errorMsg = stdErr.isEmpty() ? "Download failed or was cancelled by user." : stdErr;
        emit downloadFailed(videoId, errorMsg);
        return;
    }

    if (proc) {
        proc->deleteLater();
    }

    m_downloadRetryCount.remove(videoId);

    // Verify output file exists
    if (QFile::exists(outPath)) {
        emit downloadProgress(videoId, 1.0);
        emit downloadCompleted(videoId, outPath, audioOnly);
        std::cout << "[YT DOWNLOAD] Completed: " << outPath.toStdString() << "\n";
    } else {
        // If yt-dlp wrote to a slightly different name (e.g. extension resolved to .mkv instead of .mp4),
        // let's do a wild-card search in that directory to find any file matching the target base name
        QFileInfo fi(outPath);
        QDir dir = fi.dir();
        QStringList filters;
        filters << fi.baseName() + ".*";
        QStringList matches = dir.entryList(filters, QDir::Files);
        if (!matches.isEmpty()) {
            QString actualPath = dir.absoluteFilePath(matches.first());
            emit downloadProgress(videoId, 1.0);
            emit downloadCompleted(videoId, actualPath, audioOnly);
            std::cout << "[YT DOWNLOAD] Completed (Fallback match): " << actualPath.toStdString() << "\n";
        } else {
            emit downloadFailed(videoId, "Download completed, but output file was not found.");
        }
    }
}

} // namespace ncktv
