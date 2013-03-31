/************************************************************************/
/* JackWin is a windows version of alsa_out/alsa_in for JACK.
*/
/************************************************************************/
#include "stdio.h"
#include <windows.h>
#include <mmsystem.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <propkey.h>
#include <FunctionDiscoveryKeys_devpkey.h>
#include <Audioclient.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <jack/jack.h>
#include <conio.h>
#include <string>
//#include <unistd.h>

//#include "Interfaces.h"

jack_port_t *input_port;
jack_port_t *output_port;
IMMDevice *pDevice;
IAudioClient *pAudioClient = NULL;


#define HandleError(hres)  \
    if (FAILED(hres)) { printf("!! Failure on line: %d !!\nPress 'q' to quit.", __LINE__); \
    while(_getch() != 'q'){} exit(1); }

UINT32 bufferFrameCount;
IAudioRenderClient *pRenderClient = NULL;
IAudioCaptureClient *pCaptureClient = NULL;

/**
 * The process callback for this JACK application.
 * It is called by JACK at the appropriate times.
 */

jack_port_t** inputPorts;
int channelCount;

int
process (jack_nframes_t nframes, void *arg)
{
    // Uh copy into the buffer?
    //memcpy (out, in, sizeof (jack_default_audio_sample_t) * nframes);

    UINT32 numFramesAvailable;
    UINT32 numFramesPadding;
    BYTE *pData;
    DWORD flags = 0;

    // See how much buffer space is available.
    HRESULT hr = pAudioClient->GetCurrentPadding(&numFramesPadding);
    HandleError(hr)

        numFramesAvailable = bufferFrameCount - numFramesPadding;


    // Grab all the available space in the shared buffer.
    hr = pRenderClient->GetBuffer(numFramesAvailable, &pData);
    HandleError(hr)

        // Get next 1/2-second of data from the audio source.
    //hr = pMySource->LoadData(numFramesAvailable, pData, &flags);
    //printf("Copying %d -> %d frames\n", nframes, numFramesAvailable);
    
    int framecount = min(numFramesAvailable, nframes);

    BYTE* X = pData + sizeof(jack_default_audio_sample_t) * framecount * channelCount;
    jack_default_audio_sample_t* end = (jack_default_audio_sample_t*)X;

    for(int i = 0 ; i < channelCount ; i++){
        if(inputPorts[i] != NULL){
            //jack_default_audio_sample_t* output = _buffer;
            jack_default_audio_sample_t* output = (jack_default_audio_sample_t*)pData;
                      
            // Copy data into the buffers
            jack_default_audio_sample_t* buf = (jack_default_audio_sample_t *) jack_port_get_buffer (inputPorts[i], nframes);
            
            output += i;

            while(output < end){
                *output = *buf;
                buf++;
                output += channelCount;
            }
        }
    }
    //memcpy(pData, in, sizeof (jack_default_audio_sample_t) * framecount);
    
    HandleError(hr)


    hr = pRenderClient->ReleaseBuffer(framecount, 0);
    HandleError(hr);

    return 0;      
}

int
    process_capture (jack_nframes_t nframes, void *arg)
{
    // Uh copy into the buffer?
    //memcpy (out, in, sizeof (jack_default_audio_sample_t) * nframes);

    UINT32 frames;
    BYTE *pData;
    HRESULT hr;
    DWORD flags = 0;

    // Grab all the available space in the shared buffer.
    hr = pCaptureClient->GetBuffer(&pData, &frames, &flags, NULL, NULL);
    HandleError(hr)

    int framecount = min(frames, nframes);

    BYTE* X = pData + sizeof(jack_default_audio_sample_t) * framecount;
    jack_default_audio_sample_t* end = (jack_default_audio_sample_t*)X;

    for(int i = 0 ; i < channelCount ; i++){
        if(inputPorts[i] != NULL){
            //jack_default_audio_sample_t* output = _buffer;
            jack_default_audio_sample_t* input = (jack_default_audio_sample_t*)pData;

            // Copy data into the buffers
            jack_default_audio_sample_t* buf = (jack_default_audio_sample_t *) jack_port_get_buffer (inputPorts[i], framecount);

            input += i;

            while(input < end){
                *buf = *input;
                buf++;
                input++;
            }
        }
    }
    //memcpy(pData, in, sizeof (jack_default_audio_sample_t) * framecount);

    HandleError(hr)


        hr = pCaptureClient->ReleaseBuffer(framecount);
    HandleError(hr);

    return 0;      
}

/**
 * This is the shutdown callback for this JACK application.
 * It is called by JACK if the server ever shuts down or
 * decides to disconnect the client.
 */
void
jack_shutdown (void *arg)
{
    exit (1);
}

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

void WASAPI(WCHAR* device, bool input){
    HRESULT hr;
    REFERENCE_TIME hnsRequestedDuration = 10000000;
    REFERENCE_TIME hnsActualDuration;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    WAVEFORMATEX *pwfx = NULL;

    hr = CoInitialize(NULL);
    HandleError(hr);

    hr = CoCreateInstance(
           CLSID_MMDeviceEnumerator, NULL,
           CLSCTX_ALL, IID_IMMDeviceEnumerator,
           (void**)&pEnumerator);
    HandleError(hr)

    IMMDeviceCollection *collection;
    hr = pEnumerator->EnumAudioEndpoints(input ? EDataFlow::eCapture : EDataFlow::eRender, DEVICE_STATE_ACTIVE, &collection);
    HandleError(hr);

    UINT count;
    collection->GetCount(&count);

    bool selected = false;
    for(int i = 0 ; i < count; i++){
        hr = collection->Item(i, &pDevice);
        HandleError(hr);

        IPropertyStore *pProps;
        hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
        HandleError(hr);

        PROPVARIANT varName;
        PropVariantInit(&varName);
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        HandleError(hr);

        printf("%S\n", varName.pwszVal);
        
        if(wcscmp(device, varName.pwszVal) == 0){
            selected = true;
            break;
        }
    }

    if(!selected){
        HandleError(E_FAIL);
    }

    hr = pDevice->Activate(
                    IID_IAudioClient, CLSCTX_ALL,
                    NULL, (void**)&pAudioClient);
    HandleError(hr)

    hr = pAudioClient->GetMixFormat(&pwfx);
    HandleError(hr)

    printf("avg bits: %d\n", pwfx->nAvgBytesPerSec);
    printf("blockaln: %d\n", pwfx->nBlockAlign);
    printf("Channels: %d\n", pwfx->nChannels);
    printf("Smpl/sec: %d\n", pwfx->nSamplesPerSec);
    printf("Bit/Smpl: %d\n", pwfx->wBitsPerSample);
    printf("format  : %d\n", pwfx->wFormatTag);

    hr = pAudioClient->Initialize(
                         AUDCLNT_SHAREMODE_SHARED,
                         0,
                         hnsRequestedDuration,
                         0,
                         pwfx,
                         NULL);
    HandleError(hr)

    // Get the actual size of the allocated buffer.
    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    HandleError(hr)

    if(input){
        hr = pAudioClient->GetService(
            IID_IAudioCaptureClient,
            (void**)&pCaptureClient);
        HandleError(hr)
    }else{
        hr = pAudioClient->GetService(
                             IID_IAudioRenderClient,
                             (void**)&pRenderClient);
        HandleError(hr)
    }

    hr = pAudioClient->Start();  // Start playing.
    HandleError(hr)
}

int
wmain (int argc, WCHAR *argv[])
{
    jack_client_t *client;

    bool input = false; // argc > 4 ? (wcscmp(L"INPUT", argv[4]) == 0) : false;
    WASAPI(argv[1], input);

    if (argc < 2) {
            fprintf (stderr, "usage: JackWin <device>\n");
            return 1;
    }

    char xname[256];

    wcstombs(xname, argv[1], 256);

    /* try to become a client of the JACK server */
    if ((client = jack_client_open (xname , JackOptions::JackNoStartServer, NULL)) == 0) {
            fprintf (stderr, "jack server not running?\n");
            return 1;
    }

    /* tell the JACK server to call `process()' whenever
       there is work to be done.
    */

    if(input){
        jack_set_process_callback (client, process_capture, 0);
    }else{
        jack_set_process_callback (client, process, 0);
    }

    /* tell the JACK server to call `jack_shutdown()' if
       it ever shuts down, either entirely, or if it
       just decides to stop calling us.
    */

    jack_on_shutdown (client, jack_shutdown, 0);

    /* display the current sample rate. 
     */

    /* create two ports */

    // Ok set everything up
    channelCount = _wtoi(argv[2]);
    int channelMask = _wtoi(argv[3]);

    printf("Using %d channels w/ mask %d\n", channelCount, channelMask);


    /* labels for various speakers */
    //TODO: probably wrong for other setups, might need to do a lookup on format or something?
    int nc = 6;
    char* names[] = {
        {"left"},
        {"right"},
        {"center"},
        {"sub"},
        {"rear-left"},
        {"rear-right"},
    };

    // Just create one for now
    char name[256];
    char prefix[256];
    char _temp[256];

    if(argc > 4){
        wcstombs(prefix, argv[4], 256);
        printf("Port prefix: %s\n", prefix);
    }


    // Allocate channels

    inputPorts = new jack_port_t*[channelCount];
    
    for(int i = 0 ; i < channelCount; i++){
        if(channelMask & (1 << i)){
            printf("Allocated channel %d\n", i);

            if(i < nc){
                sprintf(_temp, "%s", names[i]);
            }else{
                sprintf(_temp, "port-%d", i);
            }

            if(argc > 4){
                sprintf(name, "%s%s", prefix, _temp);
            }

            inputPorts[i] = jack_port_register (client, name, "32 bit float mono audio", input ? JackPortIsOutput : JackPortIsInput, 0);
        }else{
            printf("Skipped channel %d\n", i);
            inputPorts[i] = NULL;
        }
    }

    /* tell the JACK server that we are ready to roll */

    if (jack_activate (client)) {
        fprintf (stderr, "cannot activate client");
        return 1;
    }

    /* connect the ports. Note: you can't do this before
       the client is activated, because we can't allow
       connections to be made to clients that aren't
       running.
    */

    while(getch() != 'q'){
    }

    jack_client_close (client);
    exit (0);
}