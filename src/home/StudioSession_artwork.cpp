#include "home/StudioSession.h"

QVariantList
StudioSession::artworkChoices() const
{
    QVariantList result;
    if (!currentProfile())
        return result;
    QSet<QString> paths;
    const auto append = [&](const QString& path)
    {
        if (result.size() >= 256 || paths.contains(path))
            return;
        QImageReader reader(path);
        const auto size = reader.size();
        if (!size.isValid() || size.width() > 16384 || size.height() > 16384 ||
            qint64(size.width()) * size.height() > 32 * 1024 * 1024)
            return;
        paths.insert(path);
        result.append(QVariantMap{{"name", QFileInfo(path).completeBaseName()},
                                  {"source", QUrl::fromLocalFile(path).toString()},
                                  {"width", size.width()},
                                  {"height", size.height()}});
    };
    const auto directory = QFileInfo(m_profilePath).absolutePath();
    const QStringList roots{QDir(directory).filePath("artwork"),
                            QDir(QFileInfo(m_live->m_defaultPath).absolutePath()).filePath("skins"),
                            QDir(QFileInfo(m_live->m_defaultPath).absolutePath()).filePath("artwork")};
    for (const auto& root : roots)
    {
        QDirIterator files(root, {"*.png", "*.jpg", "*.jpeg", "*.gif"}, QDir::Files, QDirIterator::Subdirectories);
        int inspected = 0;
        while (files.hasNext() && ++inspected <= 512 && result.size() < 256)
        {
            const auto path = files.next(), canonical = QFileInfo(path).canonicalFilePath(),
                       base = QFileInfo(root).canonicalFilePath();
            if (!canonical.isEmpty() && !base.isEmpty() && !QDir(base).relativeFilePath(canonical).startsWith("../"))
                append(canonical);
        }
    }
    for (const auto& path : m_stagedAssets)
        append(path);
    return result;
}

QVariantMap
StudioSession::browseArtwork(const QString& directory, int page)
{
    if (!currentProfile())
    {
        setError(tr("Reopen Studio before browsing images."));
        return {};
    }
    if (page < 0 || page > 7)
    {
        setError(tr("Choose an image page within 1–8."));
        return {};
    }
    const auto pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString path = directory.isEmpty() ? (QFileInfo(pictures).isDir() ? pictures : QDir::homePath()) : directory;
    const QFileInfo folder(path);
    if (!folder.isAbsolute() || !folder.isDir() || !folder.isReadable() || folder.isSymLink() ||
        path.contains(QChar::Null))
    {
        setError(tr("Choose an existing local folder. Linked folders are unavailable."));
        return {};
    }
    const auto canonical = folder.canonicalFilePath();
    if (canonical != QDir::cleanPath(folder.absoluteFilePath()))
    {
        setError(tr("Choose the folder's actual location rather than a linked path."));
        return {};
    }
    QVariantList entries;
    QDirIterator files(canonical, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    int inspected = 0;
    while (files.hasNext())
    {
        files.next();
        if (++inspected > 512)
        {
            setError(tr("This folder has more than 512 entries. Choose a smaller folder or use Advanced path."));
            return {};
        }
        const auto info = files.fileInfo();
        if (info.isDir())
            entries.append(QVariantMap{{"name", info.fileName()}, {"path", info.absoluteFilePath()}, {"folder", true}});
        else if (info.isFile() && QStringList{"png", "jpg", "jpeg", "gif"}.contains(info.suffix().toLower()) &&
                 info.size() <= 20 * 1024 * 1024)
            entries.append(
                QVariantMap{{"name", info.fileName()}, {"path", info.absoluteFilePath()}, {"folder", false}});
    }
    std::sort(entries.begin(),
              entries.end(),
              [](const QVariant& a, const QVariant& b)
              {
                  const auto l = a.toMap(), r = b.toMap();
                  if (l["folder"] != r["folder"])
                      return l["folder"].toBool();
                  return l["name"].toString() < r["name"].toString();
              });
    setError({});
    const int start = page * 64;
    return {{"directory", canonical},
            {"parent", QDir::cleanPath(canonical + "/..")},
            {"home", QDir::homePath()},
            {"pictures", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)},
            {"entries", entries.mid(start, 64)},
            {"page", page},
            {"pages", qMax(1, int((entries.size() + 63) / 64))}};
}

QString
StudioSession::artworkPreview(const QString& source)
{
    const auto relative = stageArtwork(source);
    return relative.isEmpty() || !m_model ? QString() : m_model->resolveArtwork(relative);
}

QVariantMap
StudioSession::cleanIconArtwork(const QString& source, int strength, const QString& background)
{
    if (strength < 0 || strength > 2 ||
        (!background.isEmpty() && (!QColor(background).isValid() || QColor(background).alpha() != 255)))
    {
        setError(tr("Choose a supported cleanup strength and opaque background colour."));
        return {};
    }
    const auto original = stageArtwork(source);
    if (original.isEmpty())
        return {};
    QImageReader reader(m_stagedAssets.value(original));
    reader.setAutoTransform(true);
    if (reader.supportsAnimation())
    {
        setError(tr("Icon cleanup supports still PNG/JPEG artwork. Keep animated artwork as Original."));
        return {};
    }
    const auto size = reader.size();
    if (size.width() > 512 || size.height() > 512)
        reader.setScaledSize(size.scaled(512, 512, Qt::KeepAspectRatio));
    const auto cleaned = AppArtwork::cleanBackground(reader.read(), strength, QColor(background));
    if (!cleaned.accepted)
    {
        setError({});
        return {{"accepted", false}, {"reason", cleaned.reason}};
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!cleaned.image.save(&buffer, "PNG"))
    {
        setError(tr("Could not prepare the icon preview. Original retained."));
        return {};
    }
    const auto relative = "artwork/studio-" +
                          QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) +
                          ".png";
    const auto target = QDir(m_assetStage->path()).filePath(QFileInfo(relative).fileName());
    if (!m_stagedAssets.contains(relative))
    {
        qint64 total = bytes.size();
        for (const auto& path : m_stagedAssets)
            total += QFileInfo(path).size();
        if (m_stagedAssets.size() >= 128 || total > 64 * 1024 * 1024)
        {
            setError(tr("This draft's artwork budget is 128 images / 64 MiB. Original retained."));
            return {};
        }
        QSaveFile output(target);
        if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit())
        {
            setError(tr("Could not stage icon cleanup. Nothing applied."));
            return {};
        }
        m_stagedAssets[relative] = target;
        if (m_model)
        {
            m_model->m_assetPaths[relative] = target;
            emit m_model->layoutChanged();
        }
    }
    m_iconOriginals[relative] = original;
    setError({});
    return {{"accepted", true},
            {"reason", cleaned.reason},
            {"source", QUrl::fromLocalFile(m_stagedAssets.value(relative)).toString()},
            {"preview", m_model ? m_model->resolveArtwork(relative) : QString()}};
}

QString
StudioSession::stageArtwork(const QString& source)
{
    if (!editingPermitted())
        return {};
    if (!currentProfile())
    {
        setError(tr("Reopen Studio before importing artwork."));
        return {};
    }
    const QUrl url(source);
    const auto path = url.isLocalFile() ? url.toLocalFile() : url.scheme().isEmpty() ? source : QString();
    if (path.size() > 4096 || !QFileInfo(path).isFile())
    {
        setError(tr("Choose a regular local image file."));
        return {};
    }
    QFile input(path);
    if (path.isEmpty() || !input.open(QIODevice::ReadOnly) || input.size() < 8 || input.size() > 20 * 1024 * 1024)
    {
        setError(tr("Choose a local PNG/JPEG of at most 20 MiB, or a bounded GIF of at most 2 MiB."));
        return {};
    }
    const auto data = input.readAll();
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const auto format = reader.format();
    const auto size = reader.size();
    const bool gif = format == "gif";
    QString gifError;
    if (gif && ManagedGif::decode(data, true, &gifError).isEmpty())
    {
        setError(gifError);
        return {};
    }
    if ((!gif && !QList<QByteArray>{"png", "jpeg", "jpg"}.contains(format)) || !size.isValid() ||
        size.width() > 16384 || size.height() > 16384 || qint64(size.width()) * size.height() > 32 * 1024 * 1024 ||
        reader.read().isNull())
    {
        setError(tr("Artwork must be valid PNG/JPEG (16384 pixels / 32 megapixels) or a bounded GIF."));
        return {};
    }
    const auto relative = "artwork/studio-" +
                          QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()) +
                          (gif               ? ".gif"
                           : format == "png" ? ".png"
                                             : ".jpg");
    if (m_stagedAssets.contains(relative))
        return relative;
    qint64 total = data.size();
    for (const auto& path : m_stagedAssets)
        total += QFileInfo(path).size();
    if (m_stagedAssets.size() >= 128 || total > 64 * 1024 * 1024)
    {
        setError(tr("This draft's artwork budget is 128 images / 64 MiB. Apply or discard before importing more."));
        return {};
    }
    if (!m_assetStage)
        m_assetStage = new QTemporaryDir;
    if (!m_assetStage->isValid())
    {
        setError(tr("Could not create private artwork staging."));
        return {};
    }
    const auto target = QDir(m_assetStage->path()).filePath(QFileInfo(relative).fileName());
    QSaveFile output(target);
    if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit())
    {
        setError(tr("Could not stage artwork. Nothing applied."));
        return {};
    }
    m_stagedAssets[relative] = target;
    if (m_model)
        m_model->m_assetPaths[relative] = target;
    setError({});
    if (m_model)
        emit m_model->layoutChanged();
    return relative;
}
