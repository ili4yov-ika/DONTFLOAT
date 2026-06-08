#include "../include/rubberband_offline.h"

#include <rubberband/RubberBandStretcher.h>

#include <QtCore/QtGlobal>
#include <cmath>

namespace RubberBandOffline {

QVector<float> stretchMono(const QVector<float>& input, float timeRatio, int sampleRate)
{
    if (input.isEmpty() || timeRatio <= 0.f) {
        return input;
    }
    if (qAbs(timeRatio - 1.0f) < 0.001f) {
        return input;
    }

    const int rate = sampleRate > 0 ? sampleRate : 44100;
    const int n = input.size();
    if (n <= 0) {
        return input;
    }

    const int nOut = qMax(1, static_cast<int>(std::ceil(double(n) * double(timeRatio))));

    using namespace RubberBand;

    const RubberBandStretcher::Options options =
        RubberBandStretcher::OptionEngineFiner
        | RubberBandStretcher::OptionThreadingNever
        | RubberBandStretcher::OptionChannelsApart;

    RubberBandStretcher stretcher(rate, 1, options, double(timeRatio), 1.0);

    QVector<float> inBuf = input;
    float* inChannel = inBuf.data();

    stretcher.setMaxProcessSize(qMax(1, n));
    stretcher.setExpectedInputDuration(n);

    const float* studyChannel = inChannel;
    stretcher.study(&studyChannel, static_cast<size_t>(n), true);

    stretcher.process(&inChannel, static_cast<size_t>(n), true);

    QVector<float> output(nOut, 0.f);
    float* outChannel = output.data();

    size_t totalGot = 0;
    while (true) {
        const int avail = stretcher.available();
        if (avail <= 0) {
            break;
        }
        const size_t toRead = qMin(static_cast<size_t>(avail),
                                   static_cast<size_t>(nOut) - totalGot);
        if (toRead == 0) {
            break;
        }
        float* retrievePtr = output.data() + totalGot;
        const size_t got = stretcher.retrieve(&retrievePtr, toRead);
        if (got == 0) {
            break;
        }
        totalGot += got;
    }

    output.resize(static_cast<int>(totalGot));
    return output;
}

} // namespace RubberBandOffline
