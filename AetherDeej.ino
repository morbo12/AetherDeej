
const int NUM_SLIDERS = 4;
const int analogInputs[NUM_SLIDERS] = {A0, A1, A2, A3};
const size_t SLIDER_PAYLOAD_BUFFER_SIZE = NUM_SLIDERS * 4;
const uint8_t SAMPLES_PER_SLIDER = 3;
const unsigned long SAMPLE_INTERVAL_MS = 1;
const uint8_t SEND_REPEAT_COUNT = 2;
const unsigned long SEND_REPEAT_INTERVAL_MS = 2;
const unsigned long STARTUP_STABILIZE_MS = 100;
const int SNAP_TO_MAX_PERCENT = 99;
const int SNAP_TO_MIN_PERCENT = 1;

int analogSliderValues[NUM_SLIDERS];
int sliderSampleSums[NUM_SLIDERS] = {0};
uint8_t sliderSampleCounts[NUM_SLIDERS] = {0};
bool sliderPrimed[NUM_SLIDERS] = {false};
int currentSamplingSlider = 0;
unsigned long lastSampleTime = 0;
unsigned long startupStartTime = 0;
bool startupInitialized = false;

int prevSliderValues[NUM_SLIDERS] = {0};
const int SLIDER_NOISE_THRESHOLD = 2;       // Increased threshold to reduce noise sensitivity
const unsigned long MIN_SEND_INTERVAL = 50; // Minimum milliseconds between sends
unsigned long lastSendTime = 0;
const unsigned long PERIODIC_SEND_INTERVAL = 2000; // Always send at least every 2 seconds
unsigned long lastPeriodicSend = 0;
char pendingPayload[SLIDER_PAYLOAD_BUFFER_SIZE] = {0};
uint8_t pendingSendCount = 0;
unsigned long lastRepeatSendTime = 0;

bool buildSliderPayload(char *buffer, size_t bufferSize);
void queueSliderValuesForSend(unsigned long now);
void processPendingSend(unsigned long now);
bool allSlidersPrimed();
int scaleSliderToPercent(int rawValue);

void setup()
{
    for (int i = 0; i < NUM_SLIDERS; i++)
    {
        pinMode(analogInputs[i], INPUT);
    }

    Serial.begin(115200);

    startupStartTime = millis();
}

void loop()
{
    updateSliderValues();
    unsigned long now = millis();

    if (!startupInitialized)
    {
        if (now - startupStartTime < STARTUP_STABILIZE_MS)
        {
            return;
        }

        if (!allSlidersPrimed())
        {
            return;
        }

        for (int i = 0; i < NUM_SLIDERS; i++)
        {
            prevSliderValues[i] = analogSliderValues[i];
        }

        startupInitialized = true;
        lastSendTime = now;
        lastPeriodicSend = now;
    }

    processPendingSend(now);

    if (pendingSendCount > 0)
    {
        return;
    }

    // Only send if values changed significantly AND enough time has passed
    bool changed = sliderValuesChanged();

    // Send when changed (respecting MIN_SEND_INTERVAL) OR when periodic interval elapsed
    if ((changed && (now - lastSendTime >= MIN_SEND_INTERVAL)) || (now - lastPeriodicSend >= PERIODIC_SEND_INTERVAL))
    {
        queueSliderValuesForSend(now);
        lastSendTime = now;
        lastPeriodicSend = now;

        // Update previous values only after successful send
        for (int i = 0; i < NUM_SLIDERS; i++)
        {
            prevSliderValues[i] = analogSliderValues[i];
        }
    }
    // printSliderValues(); // For debug
}

bool sliderValuesChanged()
{
    // Check if any slider has changed beyond the noise threshold
    for (int i = 0; i < NUM_SLIDERS; i++)
    {
        if (abs(analogSliderValues[i] - prevSliderValues[i]) > SLIDER_NOISE_THRESHOLD)
        {
            return true;
        }
    }
    return false;
}

void updateSliderValues()
{
    unsigned long now = millis();
    if (now - lastSampleTime < SAMPLE_INTERVAL_MS)
    {
        return;
    }

    lastSampleTime = now;

    int sample = analogRead(analogInputs[currentSamplingSlider]);
    sliderSampleSums[currentSamplingSlider] += sample;
    sliderSampleCounts[currentSamplingSlider]++;

    if (sliderSampleCounts[currentSamplingSlider] >= SAMPLES_PER_SLIDER)
    {
        analogSliderValues[currentSamplingSlider] = sliderSampleSums[currentSamplingSlider] / SAMPLES_PER_SLIDER;
        sliderPrimed[currentSamplingSlider] = true;
        sliderSampleSums[currentSamplingSlider] = 0;
        sliderSampleCounts[currentSamplingSlider] = 0;

        currentSamplingSlider++;
        if (currentSamplingSlider >= NUM_SLIDERS)
        {
            currentSamplingSlider = 0;
        }
    }
}

bool allSlidersPrimed()
{
    for (int i = 0; i < NUM_SLIDERS; i++)
    {
        if (!sliderPrimed[i])
        {
            return false;
        }
    }

    return true;
}

bool buildSliderPayload(char *buffer, size_t bufferSize)
{
    size_t offset = 0;
    buffer[0] = '\0';

    for (int i = 0; i < NUM_SLIDERS; i++)
    {
        int scaledValue = scaleSliderToPercent(analogSliderValues[i]);
        int written = snprintf(
            buffer + offset,
            bufferSize - offset,
            (i < NUM_SLIDERS - 1) ? "%d|" : "%d",
            scaledValue);

        if (written < 0)
        {
            return false;
        }

        size_t writtenSize = (size_t)written;
        if (writtenSize >= (bufferSize - offset))
        {
            return false;
        }

        offset += writtenSize;
    }

    return true;
}

int scaleSliderToPercent(int rawValue)
{
    // Use rounded integer math, then snap near endpoints to avoid edge jitter.
    long roundedPercent = ((long)rawValue * 100L + 511L) / 1023L;

    if (roundedPercent >= SNAP_TO_MAX_PERCENT)
    {
        return 100;
    }

    if (roundedPercent <= SNAP_TO_MIN_PERCENT)
    {
        return 0;
    }

    return (int)roundedPercent;
}

void queueSliderValuesForSend(unsigned long now)
{
    if (!buildSliderPayload(pendingPayload, sizeof(pendingPayload)))
    {
        return;
    }

    pendingSendCount = SEND_REPEAT_COUNT;
    lastRepeatSendTime = now - SEND_REPEAT_INTERVAL_MS;
}

void processPendingSend(unsigned long now)
{
    if (pendingSendCount == 0)
    {
        return;
    }

    if (now - lastRepeatSendTime < SEND_REPEAT_INTERVAL_MS)
    {
        return;
    }

    Serial.println(pendingPayload);
    pendingSendCount--;
    lastRepeatSendTime = now;
}

void sendSliderValues()
{
    queueSliderValuesForSend(millis());
}

void printSliderValues()
{
    for (int i = 0; i < NUM_SLIDERS; i++)
    {
        int scaledValue = scaleSliderToPercent(analogSliderValues[i]);
        char printedString[24];
        snprintf(printedString, sizeof(printedString), "Slider #%d: %d %%", i + 1, scaledValue);
        Serial.print(printedString);

        if (i < NUM_SLIDERS - 1)
        {
            Serial.print(" | ");
        }
        else
        {
            Serial.println();
        }
    }
}
