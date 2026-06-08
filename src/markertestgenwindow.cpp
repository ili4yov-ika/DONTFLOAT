#include "../include/markertestgenwindow.h"
#include "../include/waveformview.h"
#include "../include/audiofileservice.h"
#include "../include/wavwriter.h"
#include "../include/timeutils.h"
#include "../include/uiconstants.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QProgressDialog>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtGui/QCloseEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QShortcut>
#include <cmath>

namespace {

void alignWaveformViewToBarGrid(WaveformView* view, float bpm, int beatsPerBar, qint64 gridStartSample)
{
    if (!view || bpm <= 0.f) {
        return;
    }
    view->setZoomLevel(1.0f);
    const int sampleRate = view->getSampleRate();
    const float samplesPerBeat = (60.0f * sampleRate) / bpm;
    const float barLengthInQuarters =
        (beatsPerBar == 6) ? 3.f : (beatsPerBar == 12) ? 6.f : float(qMax(1, beatsPerBar));
    const float samplesPerBar = barLengthInQuarters * samplesPerBeat;
    float offset = float(gridStartSample) / samplesPerBar;
    offset = offset - std::floor(offset);
    view->setHorizontalOffset(offset);
}

} // namespace

MarkerTestGenWindow::MarkerTestGenWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("DONTFLOAT — разметка тестовых файлов"));
    setMinimumSize(900, 520);
    resize(1100, 640);

    setupUi();
    setupConnections();
}

void MarkerTestGenWindow::setupUi()
{
    auto* fileMenu = menuBar()->addMenu(tr("&Файл"));
    fileMenu->addAction(tr("&Открыть…"), this, &MarkerTestGenWindow::openAudioFile, QKeySequence::Open);
    fileMenu->addAction(tr("&Сохранить…"), this, &MarkerTestGenWindow::saveProject, QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("В&ыход"), this, &QWidget::close, QKeySequence::Quit);

    auto* editMenu = menuBar()->addMenu(tr("&Правка"));
    editMenu->addAction(tr("Привязать метки к сетке"), this, &MarkerTestGenWindow::snapMarkersToGrid);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(new QLabel(tr("BPM:"), central));
    m_bpmEdit = new QLineEdit(central);
    m_bpmEdit->setFixedWidth(72);
    toolbar->addWidget(m_bpmEdit);

    toolbar->addWidget(new QLabel(tr("Размер:"), central));
    m_barsCombo = new QComboBox(central);
    const QList<int> bpbValues = { 4, 3, 1, 2, 6, 12 };
    for (int v : bpbValues) {
        m_barsCombo->addItem(QString::number(v), v);
    }
    toolbar->addWidget(m_barsCombo);

    auto* gridBack = new QPushButton(tr("◀ сетка"), central);
    gridBack->setToolTip(tr("Сдвинуть тактовую сетку на один удар назад (Shift — вместе с метками)\n"
                           "Shift + перетаскивание ЛКМ на волне — тонкая подстройка сетки"));
    auto* gridFwd = new QPushButton(tr("сетка ▶"), central);
    gridFwd->setToolTip(tr("Сдвинуть тактовую сетку на один удар вперёд (Shift — вместе с метками)\n"
                           "Shift + перетаскивание ЛКМ на волне — тонкая подстройка сетки"));
    toolbar->addWidget(gridBack);
    toolbar->addWidget(gridFwd);

    auto* snapBtn = new QPushButton(tr("Привязать метки"), central);
    toolbar->addWidget(snapBtn);

    toolbar->addStretch();

    m_playButton = new QPushButton(tr("▶"), central);
    m_playButton->setFixedWidth(36);
    auto* stopBtn = new QPushButton(tr("■"), central);
    stopBtn->setFixedWidth(36);
    toolbar->addWidget(m_playButton);
    toolbar->addWidget(stopBtn);

    m_timeLabel = new QLabel(tr("00:00.0"), central);
    toolbar->addWidget(m_timeLabel);

    root->addLayout(toolbar);

    m_waveformView = new WaveformView(central);
    m_waveformView->setMinimumHeight(UiConstants::kWaveformContainerMinHeight);
    root->addWidget(m_waveformView, 1);

    m_horizontalScrollBar = new QScrollBar(Qt::Horizontal, central);
    m_horizontalScrollBar->setFixedHeight(20);
    root->addWidget(m_horizontalScrollBar);

    setCentralWidget(central);
    statusBar()->showMessage(tr("Откройте аудиофайл с постоянным BPM"));

    m_mediaPlayer = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_mediaPlayer->setAudioOutput(m_audioOutput);
    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setInterval(33);

    connect(gridBack, &QPushButton::clicked, this, &MarkerTestGenWindow::shiftGridBackward);
    connect(gridFwd, &QPushButton::clicked, this, &MarkerTestGenWindow::shiftGridForward);
    connect(snapBtn, &QPushButton::clicked, this, &MarkerTestGenWindow::snapMarkersToGrid);
    connect(m_playButton, &QPushButton::clicked, this, &MarkerTestGenWindow::playPause);
    connect(stopBtn, &QPushButton::clicked, this, &MarkerTestGenWindow::stopPlayback);

    auto* markerShortcut = new QShortcut(QKeySequence(Qt::Key_M), this);
    connect(markerShortcut, &QShortcut::activated, this, [this]() {
        if (!m_waveformView || m_waveformView->getAudioData().isEmpty()) {
            return;
        }
        const qint64 ms = m_waveformView->getPlaybackPosition();
        const qint64 sample = TimeUtils::msToSamples(ms, m_waveformView->getSampleRate());
        m_waveformView->addMarker(sample);
        m_dirty = true;
    });
}

void MarkerTestGenWindow::setupConnections()
{
    connect(m_bpmEdit, &QLineEdit::editingFinished, this, &MarkerTestGenWindow::onBpmEdited);
    connect(m_barsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MarkerTestGenWindow::onBeatsPerBarChanged);

    connect(m_waveformView, &WaveformView::zoomChanged, this, [this](float zoom) {
        if (!m_horizontalScrollBar) {
            return;
        }
        if (zoom <= 1.0f) {
            m_horizontalScrollBar->setMaximum(0);
            m_horizontalScrollBar->setValue(0);
            return;
        }
        const int maxValue = static_cast<int>((zoom - 1.0f) * 1000.0f);
        m_horizontalScrollBar->setMaximum(maxValue);
        m_horizontalScrollBar->setPageStep(qMax(1, maxValue / 10));
    });

    connect(m_waveformView, &WaveformView::horizontalOffsetChanged, this, [this](float offset) {
        if (!m_horizontalScrollBar || !m_waveformView) {
            return;
        }
        const float zoom = m_waveformView->getZoomLevel();
        if (zoom <= 1.0f) {
            return;
        }
        const int maxValue = m_horizontalScrollBar->maximum();
        m_horizontalScrollBar->blockSignals(true);
        m_horizontalScrollBar->setValue(qBound(0, int(offset * maxValue), maxValue));
        m_horizontalScrollBar->blockSignals(false);
    });

    connect(m_horizontalScrollBar, &QScrollBar::valueChanged, this, [this](int value) {
        if (!m_waveformView) {
            return;
        }
        const float zoom = m_waveformView->getZoomLevel();
        if (zoom <= 1.0f) {
            return;
        }
        const int maxValue = m_horizontalScrollBar->maximum();
        const float offset = maxValue > 0 ? float(value) / float(maxValue) : 0.f;
        m_waveformView->setHorizontalOffset(offset);
    });

    connect(m_waveformView, &WaveformView::positionChanged, this, &MarkerTestGenWindow::updateTimeLabel);
    connect(m_waveformView, &WaveformView::markersChanged, this, [this]() { m_dirty = true; });
    connect(m_waveformView, &WaveformView::gridStartChanged, this, [this](qint64 sample) {
        m_meta.gridStartSample = sample;
        m_dirty = true;
    });

    connect(m_playbackTimer, &QTimer::timeout, this, &MarkerTestGenWindow::updateTime);
    connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, [this](qint64 pos) {
        if (m_waveformView) {
            m_waveformView->setPlaybackPosition(pos);
        }
        updateTimeLabel(pos);
    });
}

bool MarkerTestGenWindow::maybeSave()
{
    if (!m_dirty) {
        return true;
    }
    const auto reply = QMessageBox::question(
        this, tr("Сохранить изменения?"),
        tr("Сохранить метки и аудио перед продолжением?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (reply == QMessageBox::Cancel) {
        return false;
    }
    if (reply == QMessageBox::Save) {
        saveProject();
        return !m_dirty;
    }
    return true;
}

void MarkerTestGenWindow::openAudioFile()
{
    if (!maybeSave()) {
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, tr("Открыть аудио"),
        m_audioPath.isEmpty() ? QDir::currentPath() : QFileInfo(m_audioPath).absolutePath(),
        tr("Аудио (*.wav *.mp3 *.flac);;Все файлы (*)"));
    if (path.isEmpty()) {
        return;
    }

    QProgressDialog progress(tr("Декодирование…"), QString(), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(300);
    progress.show();

    const AudioFileService::DecodeResult res = AudioFileService::decode(
        path, [&progress](int pct) { progress.setValue(pct); });
    progress.close();

    if (!res.ok || res.channels.isEmpty()) {
        QMessageBox::warning(this, tr("Ошибка"),
                             tr("Не удалось декодировать файл: %1").arg(res.error));
        return;
    }

    m_audioPath = path;
    m_markersPath = MarkersFile::markersPathForAudio(path);
    m_audioData = res.channels;
    m_waveformView->setSampleRate(res.sampleRate);
    m_waveformView->setAudioData(m_audioData);
    m_meta = MarkersFileMeta{};
    m_meta.sampleRate = res.sampleRate;
    m_meta.beatsPerBar = 4;
    m_barsCombo->setCurrentIndex(0);

    MarkersFileMeta loadedMeta;
    QVector<MarkersFileEntry> loadedEntries;
    QString loadError;
    if (QFile::exists(m_markersPath)
        && MarkersFile::readFile(m_markersPath, &loadedMeta, &loadedEntries, &loadError)) {
        m_meta = loadedMeta;
        m_bpmEdit->setText(QString::number(m_meta.bpm, 'f', 2));
        for (int i = 0; i < m_barsCombo->count(); ++i) {
            if (m_barsCombo->itemData(i).toInt() == m_meta.beatsPerBar) {
                m_barsCombo->setCurrentIndex(i);
                break;
            }
        }
        m_waveformView->setBPM(m_meta.bpm);
        m_waveformView->setBeatsPerBar(m_meta.beatsPerBar);
        m_waveformView->setGridStartSample(m_meta.gridStartSample);
        m_waveformView->setMarkers(MarkersFile::markersFromEntries(loadedEntries, res.sampleRate));
        alignViewToGrid();
        setStatus(tr("Загружены метки из %1").arg(QFileInfo(m_markersPath).fileName()));
    } else {
        if (!analyzeAndPlaceBeatMarkers()) {
            return;
        }
    }

    m_dirty = false;
    syncPlaybackSource();
    setWindowTitle(tr("DONTFLOAT — %1").arg(QFileInfo(path).fileName()));
}

bool MarkerTestGenWindow::analyzeAndPlaceBeatMarkers()
{
    if (m_audioData.isEmpty()) {
        return false;
    }

    const QVector<float> mono = AudioFileService::toMono(m_audioData);
    const int sampleRate = m_waveformView->getSampleRate();

    BPMAnalyzer::AnalysisOptions options;
    const BPMAnalyzer::AnalysisResult analysis =
        BPMAnalyzer::analyzeBPM(mono, sampleRate, options);

    if (analysis.bpm <= 0.f) {
        QMessageBox::warning(this, tr("Анализ BPM"),
                             tr("Не удалось определить BPM. Укажите BPM вручную."));
        return false;
    }

    m_meta.bpm = analysis.bpm;
    m_meta.gridStartSample = analysis.gridStartSample;
    m_meta.beatsPerBar = m_barsCombo->currentData().toInt();
    m_meta.sampleRate = sampleRate;

    m_bpmEdit->setText(QString::number(analysis.bpm, 'f', 2));
    m_waveformView->setBPM(analysis.bpm);
    m_waveformView->setBeatInfo(analysis.beats);
    m_waveformView->setGridStartSample(analysis.gridStartSample);
    m_waveformView->setBeatsPerBar(m_meta.beatsPerBar);
    m_waveformView->setBeatsAligned(true);

    m_waveformView->setMarkers(buildBeatMarkersFromGrid());
    alignViewToGrid();
    m_waveformView->update();

    setStatus(tr("BPM: %1 — метки на каждой доле").arg(analysis.bpm, 0, 'f', 2), 5000);
    return true;
}

QVector<Marker> MarkerTestGenWindow::buildBeatMarkersFromGrid() const
{
    QVector<Marker> markers;
    if (!m_waveformView || m_audioData.isEmpty()) {
        return markers;
    }

    const int sampleRate = m_waveformView->getSampleRate();
    const float bpm = m_waveformView->getBPM();
    const int beatsPerBar = m_waveformView->getBeatsPerBar();
    const qint64 gridStart = m_waveformView->getGridStartSample();
    const qint64 totalSamples = m_audioData[0].size();

    if (bpm <= 0.f || sampleRate <= 0) {
        return markers;
    }

    const float beatInterval = (60.0f * sampleRate) / bpm;
    markers.append(Marker(0, true, sampleRate));

    qint64 pos = gridStart;
    while (pos < totalSamples) {
        if (pos > 0) {
            Marker m(pos, sampleRate);
            m.originalPosition = pos;
            markers.append(m);
        }
        pos += static_cast<qint64>(std::lround(beatInterval));
    }

    const qint64 endPos = totalSamples - 1;
    if (endPos > 0) {
        Marker endMarker(endPos, false, true, sampleRate);
        endMarker.originalPosition = endPos;
        markers.append(endMarker);
    }

    return markers;
}

void MarkerTestGenWindow::alignViewToGrid()
{
    if (!m_waveformView) {
        return;
    }
    alignWaveformViewToBarGrid(
        m_waveformView,
        m_waveformView->getBPM(),
        m_waveformView->getBeatsPerBar(),
        m_waveformView->getGridStartSample());
}

void MarkerTestGenWindow::shiftGridByBeats(int beatDelta)
{
    if (!m_waveformView || m_audioData.isEmpty()) {
        return;
    }

    const float bpm = m_waveformView->getBPM();
    const int sampleRate = m_waveformView->getSampleRate();
    if (bpm <= 0.f || sampleRate <= 0) {
        return;
    }

    const qint64 beatSamples = qMax<qint64>(1, qRound((60.0f * sampleRate) / bpm));
    const bool moveMarkers = QApplication::keyboardModifiers() & Qt::ShiftModifier;
    const qint64 oldGrid = m_waveformView->getGridStartSample();
    const qint64 maxGrid = qMax<qint64>(0, m_audioData[0].size() - 1);
    const qint64 newGrid = qBound<qint64>(0, oldGrid + beatDelta * beatSamples, maxGrid);
    if (newGrid == oldGrid) {
        return;
    }

    m_waveformView->shiftGridBySamples(newGrid - oldGrid, moveMarkers);
    alignViewToGrid();

    setStatus(tr("Сетка сдвинута %1 на 1 удар")
                  .arg(beatDelta < 0 ? tr("назад") : tr("вперёд")));
}

void MarkerTestGenWindow::shiftGridBackward()
{
    shiftGridByBeats(-1);
}

void MarkerTestGenWindow::shiftGridForward()
{
    shiftGridByBeats(1);
}

void MarkerTestGenWindow::snapMarkersToGrid()
{
    if (!m_waveformView) {
        return;
    }
    const QVector<Marker> snapped = m_waveformView->snapMarkersToGrid(m_waveformView->getMarkers());
    m_waveformView->setMarkers(snapped);
    m_dirty = true;
    setStatus(tr("Метки привязаны к тактовой сетке"));
}

void MarkerTestGenWindow::onBpmEdited()
{
    if (!m_waveformView) {
        return;
    }
    bool ok = false;
    const float bpm = m_bpmEdit->text().toFloat(&ok);
    if (!ok || bpm <= 0.f) {
        return;
    }
    m_meta.bpm = bpm;
    m_waveformView->setBPM(bpm);
    m_dirty = true;
}

void MarkerTestGenWindow::onBeatsPerBarChanged(int)
{
    if (!m_waveformView) {
        return;
    }
    const int bpb = m_barsCombo->currentData().toInt();
    m_meta.beatsPerBar = bpb;
    m_waveformView->setBeatsPerBar(bpb);
    alignViewToGrid();
    m_dirty = true;
}

void MarkerTestGenWindow::saveProject()
{
    if (m_audioPath.isEmpty() || m_audioData.isEmpty() || !m_waveformView) {
        return;
    }

    const QString defaultAudio = m_audioPath;
    const QString audioOut = QFileDialog::getSaveFileName(
        this, tr("Сохранить аудиофайл"),
        defaultAudio,
        tr("WAV (*.wav);;MP3 — копия исходника (*.mp3);;Все файлы (*)"));
    if (audioOut.isEmpty()) {
        return;
    }

    const QFileInfo outInfo(audioOut);
    const QString markersOut = outInfo.absolutePath() + QLatin1Char('/')
                               + outInfo.completeBaseName() + QStringLiteral("_markers.txt");

    if (outInfo.suffix().compare(QStringLiteral("wav"), Qt::CaseInsensitive) == 0) {
        WavWriter::WriteOptions opts;
        opts.format = WavWriter::SampleFormat::Float32;
        QString err;
        if (!WavWriter::writeFile(audioOut, m_waveformView->getAudioData(),
                                  m_waveformView->getSampleRate(), &err, opts)) {
            QMessageBox::warning(this, tr("Ошибка"), err);
            return;
        }
    } else if (QFileInfo(m_audioPath).absoluteFilePath() != QFileInfo(audioOut).absoluteFilePath()) {
        if (QFile::exists(audioOut)) {
            QFile::remove(audioOut);
        }
        if (!QFile::copy(m_audioPath, audioOut)) {
            QMessageBox::warning(this, tr("Ошибка"),
                                 tr("Не удалось скопировать аудиофайл."));
            return;
        }
    }

    m_meta.bpm = m_waveformView->getBPM();
    m_meta.beatsPerBar = m_waveformView->getBeatsPerBar();
    m_meta.sampleRate = m_waveformView->getSampleRate();
    m_meta.gridStartSample = m_waveformView->getGridStartSample();

    QString err;
    if (!MarkersFile::writeFile(markersOut, m_meta, m_waveformView->getMarkers(),
                                m_meta.sampleRate, &err)) {
        QMessageBox::warning(this, tr("Ошибка"), err);
        return;
    }

    m_audioPath = audioOut;
    m_markersPath = markersOut;
    m_dirty = false;
    setStatus(tr("Сохранено: %1 и %2")
                  .arg(QFileInfo(audioOut).fileName(), QFileInfo(markersOut).fileName()), 6000);
}

void MarkerTestGenWindow::syncPlaybackSource()
{
    if (!m_mediaPlayer || m_audioPath.isEmpty()) {
        return;
    }
    m_mediaPlayer->setSource(QUrl::fromLocalFile(m_audioPath));
    m_mediaPlayer->setPosition(0);
}

void MarkerTestGenWindow::playPause()
{
    if (!m_mediaPlayer) {
        return;
    }
    if (!m_isPlaying) {
        m_isPlaying = true;
        m_mediaPlayer->play();
        m_playbackTimer->start();
        m_playButton->setText(tr("❚❚"));
    } else {
        m_isPlaying = false;
        m_mediaPlayer->pause();
        m_playbackTimer->stop();
        m_playButton->setText(tr("▶"));
    }
}

void MarkerTestGenWindow::stopPlayback()
{
    if (!m_mediaPlayer) {
        return;
    }
    m_isPlaying = false;
    m_mediaPlayer->stop();
    m_playbackTimer->stop();
    m_playButton->setText(tr("▶"));
    updateTimeLabel(0);
}

void MarkerTestGenWindow::updateTime()
{
    if (m_mediaPlayer) {
        updateTimeLabel(m_mediaPlayer->position());
    }
}

void MarkerTestGenWindow::updateTimeLabel(qint64 ms)
{
    if (!m_timeLabel || !m_waveformView) {
        return;
    }
    const QString timeStr = TimeUtils::formatTimeAndBars(
        ms,
        m_waveformView->getBPM(),
        m_waveformView->getBeatsPerBar(),
        m_waveformView->getSampleRate(),
        m_waveformView->getGridStartSample());
    m_timeLabel->setText(timeStr);
}

void MarkerTestGenWindow::setStatus(const QString& text, int timeoutMs)
{
    statusBar()->showMessage(text, timeoutMs);
}

void MarkerTestGenWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space) {
        playPause();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MarkerTestGenWindow::closeEvent(QCloseEvent* event)
{
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}
