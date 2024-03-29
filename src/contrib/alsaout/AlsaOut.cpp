//////////////////////////////////////////////////////////////////////////////
// Copyright  2007, Daniel nnerby
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////
#include "AlsaOut.h"

AlsaOut::AlsaOut()
 :waveHandle(NULL)
 ,maxBuffers(32)
 ,currentVolume(1.0)
 ,addToRemovedBuffers(false)
 ,device("default")
 ,output(NULL)
{
#ifdef _DEBUG
	std::cerr << "AlsaOut::AlsaOut() called" << std::endl;
#endif
	int err;
	if ((err = snd_pcm_open (&waveHandle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		std::cerr << "AlsaOut: cannot open audio device 'default' :" << snd_strerror(err) << std::endl;
		return;
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: audio device opened" << std::endl;
	}
	#endif
	
	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		std::cerr << "AlsaOut: cannot allocate hardware parameter structure " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: audio interface prepared for use" << std::endl;
	}
	#endif
	
	if ((err = snd_pcm_hw_params_any (waveHandle, hw_params)) < 0) {
		std::cerr << "AlsaOut: cannot initialize hardware parameter structure " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: audio interface prepared for use" << std::endl;
	}
	#endif
	
	if ((err = snd_pcm_hw_params_set_access (waveHandle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		std::cerr << "AlsaOut: cannot set access type " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: access type set" << std::endl;
	}
	#endif
	
	if ((err = snd_pcm_hw_params_set_format (waveHandle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		std::cerr << "AlsaOut: cannot set sample format " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: sample format set" << std::endl;
	}
	#endif
	
	if ((err = snd_pcm_hw_params_set_rate_resample (waveHandle, hw_params, 44100)) < 0) {
		std::cerr << "AlsaOut: cannot set sample rate " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: sample rate set" << std::endl;
	}
	#endif
	
	if ((err = snd_pcm_hw_params_set_channels (waveHandle, hw_params, 2)) < 0) {
		std::cerr << "AlsaOut: cannot set channel count " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: channel count set" << std::endl;
	}
	#endif
	
	if ((err = snd_pcm_hw_params (waveHandle, hw_params)) < 0) {
		std::cerr << "AlsaOut: cannot set parameters " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: parameters set" << std::endl;
	}
	#endif
	
	snd_pcm_hw_params_free (hw_params);
	
	if ((err = snd_pcm_prepare (waveHandle)) < 0) {
		std::cerr << "cannot prepare audio interface for use " << snd_strerror(err) << std::endl;
		exit (1);
	}
	#ifdef _DEBUG
	else {
		std::cerr << "AlsaOut: audio interface prepared for use" << std::endl;
	}
	std::cerr << "waveHandle: " << waveHandle << std::endl;
	#endif
}

AlsaOut::~AlsaOut(){
    this->ClearBuffers();

    if(this->waveHandle!=NULL){
        snd_pcm_close(this->waveHandle);
        this->waveHandle    = NULL;
    }

}

snd_pcm_t* AlsaOut::getWaveHandle() {
	return this->waveHandle;
}

void AlsaOut::Destroy(){
    delete this;
}
/*
void WaveOut::Initialize(IPlayer *player){
    this->player    = player;
}
*/
void AlsaOut::Pause(){
    snd_pcm_pause(this->waveHandle, 1);
}

void AlsaOut::Resume(){
    snd_pcm_pause(this->waveHandle, 0);
}

void AlsaOut::SetVolume(double volume){
    /*if(this->waveHandle){
        DWORD newVolume = (DWORD)(volume*65535.0);
        newVolume   += newVolume*65536;

        waveOutSetVolume(this->waveHandle,newVolume);
    }
    this->currentVolume = volume;*/ //TODO: Write alsa SetVolume() function
}

void AlsaOut::ClearBuffers(){
	 snd_pcm_drop(this->waveHandle);
    snd_pcm_reset(this->waveHandle);
}

void AlsaOut::RemoveBuffer(AlsaOutBuffer *buffer){
    BufferList clearBuffers;
    {
        boost::mutex::scoped_lock lock(this->mutex);
        bool found(false);
        for(BufferList::iterator buf=this->buffers.begin();buf!=this->buffers.end() && !found;){
            if(buf->get()==buffer){
//                if( !(*buf)->ReadyToRelease() ){
                    this->removedBuffers.push_back(*buf);
//                }
                clearBuffers.push_back(*buf);
                buf=this->buffers.erase(buf);
                found=true;
            }else{
                ++buf;
            }
        }
    }
}

void AlsaOut::ReleaseBuffers(){
    BufferList clearBuffers;
    {
        boost::mutex::scoped_lock lock(this->mutex);
        for(BufferList::iterator buf=this->removedBuffers.begin();buf!=this->removedBuffers.end();){
            clearBuffers.push_back(*buf);
            buf = this->removedBuffers.erase(buf);
        }
    }

}

bool AlsaOut::PlayBuffer(IBuffer *buffer,IPlayer *player){
#ifdef _DEBUG
	std::cerr << "AlsaOut::PlayBuffer()" << std::endl;
	std::cerr << "Buffer: " << buffer << std::endl;
#endif
    size_t bufferSize  = 0;
    {
        boost::mutex::scoped_lock lock(this->mutex);
        bufferSize  = this->buffers.size();
    }
#ifdef _DEBUG
	std::cerr << "buffer size: " << bufferSize << std::endl;
#endif

    // if the format should change, wait for all buffers to be released
    if(bufferSize>0 && (this->currentChannels!=buffer->Channels() || this->currentSampleRate!=buffer->SampleRate())){
#ifdef _DEBUG
	std::cerr << "format changed" << std::endl;
#endif
        // Format has changed
//        this->player->Notify()
        return false;
    }


    if(bufferSize<this->maxBuffers){
        // Start by checking the format
        this->SetFormat(buffer);
#ifdef _DEBUG
	std::cerr << "format set" << std::endl;
#endif

        // Add to the waveout internal buffers
        AlsaOutBufferPtr alsaBuffer(new AlsaOutBuffer(this,buffer,player));
#ifdef _DEBUG
	std::cerr << "alsaBuffer created" << std::endl;
#endif

        // Header should now be prepared, lets add to waveout
        if( alsaBuffer->AddToOutput() ){
#ifdef _DEBUG
	std::cerr << "added to internal buffer" << std::endl;
#endif
            // Add to the buffer list
            {
                boost::mutex::scoped_lock lock(this->mutex);
                this->buffers.push_back(alsaBuffer);
#ifdef _DEBUG
	std::cerr << "added to buffer list" << std::endl;
#endif
            }
            return true;
        }

    }
#ifdef _DEBUG
	std::cerr << "buffer size too big" << std::endl;
#endif

    return false;
}

void AlsaOut::SetFormat(IBuffer *buffer){
    if(this->currentChannels!=buffer->Channels() || this->currentSampleRate!=buffer->SampleRate() ||this->waveHandle==NULL){
        this->currentChannels   = buffer->Channels();
        this->currentSampleRate = buffer->SampleRate();

        // Close old waveout
        if(this->waveHandle!=NULL){
            snd_pcm_close(this->waveHandle);
            this->waveHandle    = NULL;
        }
/*
        // Create a new waveFormat
	    ZeroMemory(&this->waveFormat, sizeof(this->waveFormat));
        DWORD speakerconfig;

        // Set speaker configuration
        switch(buffer->Channels()){
            case 1:
                speakerconfig = KSAUDIO_SPEAKER_MONO;
                break;
            case 2:
                speakerconfig = KSAUDIO_SPEAKER_STEREO;
                break;
            case 4:
                speakerconfig = KSAUDIO_SPEAKER_QUAD;
                break;
            case 5:
                speakerconfig = (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT  | SPEAKER_BACK_RIGHT);
                break;
            case 6:
                speakerconfig = KSAUDIO_SPEAKER_5POINT1;
                break;
            default:
                speakerconfig = 0;
        }

        this->waveFormat.Format.cbSize					= 22;
        this->waveFormat.Format.wFormatTag				= WAVE_FORMAT_EXTENSIBLE;
        this->waveFormat.Format.nChannels				= (WORD)buffer->Channels();
        this->waveFormat.Format.nSamplesPerSec			= (DWORD)buffer->SampleRate();
        this->waveFormat.Format.wBitsPerSample			= 32;
        this->waveFormat.Format.nBlockAlign			    = (this->waveFormat.Format.wBitsPerSample/8) * this->waveFormat.Format.nChannels;
        this->waveFormat.Format.nAvgBytesPerSec		    = ((this->waveFormat.Format.wBitsPerSample/8) * this->waveFormat.Format.nChannels) * this->waveFormat.Format.nSamplesPerSec; //Compute using nBlkAlign * nSamp/Sec

        // clangen: wValidBitsPerSample/wReserved/wSamplesPerBlock are a union,
        // so don't set wReserved or wSamplesPerBlock to 0 after assigning
        // wValidBitsPerSample. (Vista bug)
        this->waveFormat.Samples.wValidBitsPerSample	= 32;
        this->waveFormat.dwChannelMask                  = speakerconfig;
        this->waveFormat.SubFormat                      = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        */
        this->waveFormat						= SND_PCM_FORMAT_FLOAT_LE;
        this->waveAccess						= SND_PCM_ACCESS_RW_INTERLEAVED;

        int err;
		//Open the device
			if ((err = snd_pcm_open(&this->waveHandle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
	         printf("Playback open error: %s\n", snd_strerror(err));
	         return;
         }
		//Set simple parameters
			if (( err = snd_pcm_set_params(
					this->waveHandle,
					this->waveFormat,
					this->waveAccess,
					this->currentChannels,
					this->currentSampleRate,
					1,		//Allow alsa-lib software resampling
					500000)	//Required overall latency (us) /* 0.5s */
					) > 0)	{	//If an error...
						printf("Playback open error: %s\n", snd_strerror(err));
	               exit(EXIT_FAILURE);
	            }

        // Set the volume if it's not already set
        this->SetVolume(this->currentVolume);
    }
}
