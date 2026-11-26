#include "KodiMedia.h"

void
KodiMedia::scanMovies()
{
    startScan("Video");
}
void
KodiMedia::scanMusic()
{
    startScan("Music");
}
void
KodiMedia::startScan(const QString& kind)
{
    if (m_busy || m_playbackActive || !m_connected)
    {
        m_scanStatus = m_playbackActive ? "Stop playback before scanning, then retry."
                       : m_busy
                           ? "Kodi is finishing another operation. Wait, then retry the scan."
                           : "Connect to Kodi before scanning. Refresh the library connection, then retry. " + m_status;
        emit changed();
        return;
    }
    ++m_generation;
    m_busy = true;
    m_stage = 2;
    m_scanKind = kind;
    m_observedScan = false;
    m_scanPolls = 0;
    m_scanStatus = kind == "Music" ? "Requesting a scan of this profile's music folders and embedded tags…"
                                   : "Requesting a scan of this profile's configured video folders…";
    emit changed();
    request(kind == "Music" ? "AudioLibrary.Scan" : "VideoLibrary.Scan");
}
void
KodiMedia::refreshSources()
{
    if (m_busy || m_playbackActive)
        return;
    m_sourcePreviewReady = false;
    m_busy = true;
    m_stage = 22;
    m_sourceStatus = "Reading this profile's Kodi sources and installed scrapers…";
    emit changed();
    request("Isolde.GetSources");
}
void
KodiMedia::previewSource(const QVariantMap& source)
{
    if (!canPreviewSource())
        return;
    m_sourceDraft = source;
    m_sourcePreviewReady = false;
    m_sourceRevision.clear();
    m_busy = true;
    m_stage = 23;
    m_sourceStatus = "Validating source draft · nothing saved…";
    emit changed();
    request("Isolde.PreviewSource", {{"source", QJsonObject::fromVariantMap(source)}});
}

bool
KodiMedia::canPreviewSource() const
{
    return !m_busy && !m_playbackActive && m_sourceGuard && m_sourceGuard();
}
void
KodiMedia::applySource(const QVariantMap& source)
{
    if (m_busy || m_playbackActive || !m_sourceGuard || !m_sourceGuard())
        return;
    if (!m_sourcePreviewReady || source != m_sourceDraft || m_sourceRevision.size() != 64)
    {
        m_sourceStatus = "Preview this exact source draft before saving.";
        emit changed();
        return;
    }
    m_sourcePreviewReady = false;
    m_busy = true;
    m_stage = 24;
    m_sourceStatus = "Applying the confirmed source settings through Kodi…";
    emit changed();
    request("Isolde.ApplySource", {{"source", QJsonObject::fromVariantMap(source)}, {"revision", m_sourceRevision}});
}
void
KodiMedia::cancelSourcePreview()
{
    m_sourcePreviewReady = false;
    m_sourceDraft.clear();
    m_sourceRevision.clear();
    emit changed();
}

bool
KodiMedia::clean()
{
    if (m_busy || m_playbackActive || !m_connected)
    {
        m_cleanStatus = m_playbackActive ? "Stop playback before cleaning, then retry."
                           : m_busy
                              ? "Kodi is finishing another operation. Wait, then retry the clean."
                              : "Connect to Kodi before cleaning. Refresh the library connection, then retry. " + m_status;
        emit changed();
        return false;
    }
    ++m_generation;
    m_busy = true;
    m_stage = 30;
    m_cleanStatus = "Requesting Kodi clean the library…";
    emit changed();
    request("VideoLibrary.Clean", {{"showdialogs", false}});
    return true;
}
