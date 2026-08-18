#ifndef PITCHNOTESPLITCOMMAND_H
#define PITCHNOTESPLITCOMMAND_H

#include <QUndoCommand>
#include <functional>
#include "pitchdetector.h"

/**
 * @brief Undo/redo разреза одной ноты пианоролла на две части.
 *
 * Работает с базовой моделью нот (координаты исходного аудио) в MainWindow:
 * нота \a noteIndex укорачивается до точки реза, вторая половина вставляется
 * следующим элементом. Высота, определённая высота и уверенность наследуются
 * обеими частями, поэтому сам по себе разрез звук не меняет — он лишь даёт
 * править половины по отдельности.
 *
 * Индексы последующих нот сдвигаются, но undo-стек строго LIFO: команды правки
 * высоты, добавленные после разреза, отменяются раньше него, поэтому их
 * индексы остаются корректными.
 */
class PitchNoteSplitCommand : public QUndoCommand
{
public:
    PitchNoteSplitCommand(QVector<PitchDetector::PitchNote>* baseNotes,
                          int noteIndex,
                          qint64 splitSample,
                          const QString& text,
                          std::function<void()> onApplied = {},
                          QUndoCommand* parent = nullptr)
        : QUndoCommand(text, parent)
        , m_baseNotes(baseNotes)
        , m_noteIndex(noteIndex)
        , m_splitSample(splitSample)
        , m_onApplied(std::move(onApplied))
    {
    }

    void redo() override
    {
        if (!isIndexValid()) {
            return;
        }
        PitchDetector::PitchNote& head = (*m_baseNotes)[m_noteIndex];
        if (m_splitSample <= head.startSample || m_splitSample >= head.endSample) {
            return;
        }

        m_originalNote = head;
        PitchDetector::PitchNote tail = head;
        // Отрезок исходного звука делится в той же точке, что и сама нота:
        // иначе половинки после разреза звучали бы не своими кусками
        const qint64 sourceCut = head.sourceStart() + (m_splitSample - head.startSample);
        head.sourceStartSample = head.sourceStart();
        head.sourceEndSample = sourceCut;
        tail.sourceStartSample = sourceCut;
        tail.sourceEndSample = m_originalNote.sourceEnd();
        head.endSample = m_splitSample;
        tail.startSample = m_splitSample;
        m_baseNotes->insert(m_noteIndex + 1, tail);
        m_applied = true;
        notify();
    }

    void undo() override
    {
        if (!m_applied || !isIndexValid() || m_noteIndex + 1 >= m_baseNotes->size()) {
            return;
        }
        m_baseNotes->remove(m_noteIndex + 1);
        (*m_baseNotes)[m_noteIndex] = m_originalNote;
        m_applied = false;
        notify();
    }

private:
    bool isIndexValid() const
    {
        return m_baseNotes && m_noteIndex >= 0 && m_noteIndex < m_baseNotes->size();
    }

    void notify() const
    {
        if (m_onApplied) {
            m_onApplied();
        }
    }

    QVector<PitchDetector::PitchNote>* m_baseNotes;
    int m_noteIndex;
    qint64 m_splitSample;
    PitchDetector::PitchNote m_originalNote;
    bool m_applied = false;
    std::function<void()> m_onApplied;
};

#endif // PITCHNOTESPLITCOMMAND_H
