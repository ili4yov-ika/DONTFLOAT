#ifndef PITCHNOTEEDITCOMMAND_H
#define PITCHNOTEEDITCOMMAND_H

#include <QUndoCommand>
#include <QPointer>
#include <functional>
#include "pitchgridwidget.h"
#include "pitchdetector.h"

/**
 * @brief Undo/redo изменения высоты одной ноты на пианоролле.
 *
 * Обновляет базовую модель нот (в MainWindow) и отображение в PitchGridWidget.
 * Последовательные правки одной и той же ноты объединяются (mergeWith).
 */
class PitchNoteEditCommand : public QUndoCommand
{
public:
    PitchNoteEditCommand(PitchGridWidget* widget,
                         QVector<PitchDetector::PitchNote>* baseNotes,
                         int noteIndex,
                         int oldPitch,
                         int newPitch,
                         const QString& text,
                         std::function<void()> onApplied = {},
                         QUndoCommand* parent = nullptr)
        : QUndoCommand(text, parent)
        , m_widget(widget)
        , m_baseNotes(baseNotes)
        , m_noteIndex(noteIndex)
        , m_oldPitch(oldPitch)
        , m_newPitch(newPitch)
        , m_onApplied(std::move(onApplied))
    {
    }

    void undo() override { applyPitch(m_oldPitch); }
    void redo() override { applyPitch(m_newPitch); }

    int id() const override { return kCommandId; }

    bool mergeWith(const QUndoCommand* other) override
    {
        const auto* cmd = static_cast<const PitchNoteEditCommand*>(other);
        if (cmd->m_noteIndex != m_noteIndex) {
            return false;
        }
        m_newPitch = cmd->m_newPitch;
        return true;
    }

private:
    static constexpr int kCommandId = 0xA10E;

    void applyPitch(int pitch)
    {
        if (m_baseNotes && m_noteIndex >= 0 && m_noteIndex < m_baseNotes->size()) {
            (*m_baseNotes)[m_noteIndex].midiPitch = pitch;
        }
        if (m_widget) {
            m_widget->setNotePitch(m_noteIndex, pitch);
        }
        if (m_onApplied) {
            m_onApplied();
        }
    }

    QPointer<PitchGridWidget> m_widget;
    QVector<PitchDetector::PitchNote>* m_baseNotes;
    int m_noteIndex;
    int m_oldPitch;
    int m_newPitch;
    std::function<void()> m_onApplied;
};

#endif // PITCHNOTEEDITCOMMAND_H
