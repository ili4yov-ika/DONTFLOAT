#ifndef PITCHNOTEMOVECOMMAND_H
#define PITCHNOTEMOVECOMMAND_H

#include <QUndoCommand>
#include <QPointer>
#include <functional>
#include "pitchgridwidget.h"
#include "pitchdetector.h"

/**
 * @brief Undo/redo сдвига ноты по времени (drag по горизонтали).
 *
 * Длина ноты сохраняется — двигается весь блок. Последовательные сдвиги одной
 * и той же ноты объединяются, чтобы один жест давал одну запись в стеке.
 */
class PitchNoteMoveCommand : public QUndoCommand
{
public:
    PitchNoteMoveCommand(PitchGridWidget* widget,
                         QVector<PitchDetector::PitchNote>* baseNotes,
                         int noteIndex,
                         qint64 oldStartSample,
                         qint64 newStartSample,
                         const QString& text,
                         std::function<void()> onApplied = {},
                         QUndoCommand* parent = nullptr)
        : QUndoCommand(text, parent)
        , m_widget(widget)
        , m_baseNotes(baseNotes)
        , m_noteIndex(noteIndex)
        , m_oldStartSample(oldStartSample)
        , m_newStartSample(newStartSample)
        , m_onApplied(std::move(onApplied))
    {
    }

    void undo() override { applyStart(m_oldStartSample); }
    void redo() override { applyStart(m_newStartSample); }

    int id() const override { return kCommandId; }

    bool mergeWith(const QUndoCommand* other) override
    {
        const auto* next = dynamic_cast<const PitchNoteMoveCommand*>(other);
        if (!next || next->m_noteIndex != m_noteIndex || next->m_baseNotes != m_baseNotes) {
            return false;
        }
        m_newStartSample = next->m_newStartSample;
        return true;
    }

private:
    void applyStart(qint64 startSample)
    {
        if (!m_baseNotes || m_noteIndex < 0 || m_noteIndex >= m_baseNotes->size()) {
            return;
        }
        PitchDetector::PitchNote& note = (*m_baseNotes)[m_noteIndex];
        const qint64 length = note.endSample - note.startSample;
        note.startSample = startSample;
        note.endSample = startSample + length;

        if (m_widget) {
            m_widget->setNotes(*m_baseNotes);
        }
        if (m_onApplied) {
            m_onApplied();
        }
    }

    static constexpr int kCommandId = 0x504E4D56;  // 'PNMV'

    QPointer<PitchGridWidget> m_widget;
    QVector<PitchDetector::PitchNote>* m_baseNotes = nullptr;
    int m_noteIndex = -1;
    qint64 m_oldStartSample = 0;
    qint64 m_newStartSample = 0;
    std::function<void()> m_onApplied;
};

#endif // PITCHNOTEMOVECOMMAND_H
