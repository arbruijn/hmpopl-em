
mergeInto(LibraryManager.library, {
  myOpenAudio:function(desired, obtained) {

	function AudioDataDestination(channels, sampleRate, readFn) {
        // Initialize the audio output.
        var audio = new Audio();
        audio.mozSetup(channels, sampleRate);

        var currentWritePosition = 0;
        var prebufferSize = sampleRate * channels / 4; // buffer 250ms
        var tail = null, tailPosition;
		var soundData = new Float32Array(prebufferSize);
		var soundDataLen = 0;

        // The function called with regular interval to populate 
        // the audio output buffer.
        function updater() {
          var written;
          // Check if some data was not written in previous attempts.
          if(tail) {
            written = audio.mozWriteAudio(tail.subarray(tailPosition, soundDataLen));
            currentWritePosition += written;
            tailPosition += written;
            if(tailPosition < soundDataLen) {
              // Not all the data was written, saving the tail...
              return; // ... and exit the function.
            }
            tail = null;
          }

          // Check if we need add some data to the audio output.
          var currentPosition = audio.mozCurrentSampleOffset();
          var available = currentPosition + prebufferSize - currentWritePosition;
          if(available > 0) {
            // Request some sound data from the callback function.
            var soundDataLen = readFn(soundData, available);

            // Writting the data.
            written = audio.mozWriteAudio(soundData.subarray(0, soundDataLen));
            if(written < soundDataLen) {
              // Not all the data was written, saving the tail.
              tail = soundData;
              tailPosition = written;
            }
            currentWritePosition += written;
          }
        }
		var timerId = null;
		this.start = function() { if (timerId !== null) return; timerId = setInterval(updater, 50); }
		this.stop = function() { if (timerId === null) return; clearInterval(timerId); timerId = null; }
      }
      //SDL.allocateChannels(32);
      // FIXME: Assumes 16-bit audio
      assert(obtained === 0, 'Cannot return obtained SDL audio params');
      SDL.audio = {
        freq: HEAPU32[(((desired)+(SDL.structs.AudioSpec.freq))>>2)],
        format: HEAPU16[(((desired)+(SDL.structs.AudioSpec.format))>>1)],
        channels: HEAPU8[(((desired)+(SDL.structs.AudioSpec.channels))|0)],
        samples: HEAPU16[(((desired)+(SDL.structs.AudioSpec.samples))>>1)],
        callback: HEAPU32[(((desired)+(SDL.structs.AudioSpec.callback))>>2)],
        userdata: HEAPU32[(((desired)+(SDL.structs.AudioSpec.userdata))>>2)],
        paused: true,
        timer: null
      };
      var totalSamples = SDL.audio.freq*SDL.audio.channels;
      SDL.audio.bufferSize = totalSamples*2; // hardcoded 16-bit audio
	  SDL.audio.buffer = _malloc(SDL.audio.bufferSize);
      try {
  	    SDL.audio.dest = new AudioDataDestination(SDL.audio.channels, SDL.audio.freq, function(buf, n) {
          var ptr = SDL.audio.buffer;
          Runtime.dynCall('viii', SDL.audio.callback, [SDL.audio.userdata, ptr, n * 2]);
  		  ptr >>= 1;
          for (var i = 0; i < n; i++)
            buf[i] = HEAP16[ptr++] / 0x8000; // hardcoded 16-bit audio, signed
		  return n;
        });
      } catch (e) {
		console.log(e);
        SDL.audio = null;
      }
      return SDL.audio ? 0 : -1;
    },
  myPauseAudio:function(pauseOn) {
	  pauseOn = !!pauseOn;
      if (SDL.audio.paused !== pauseOn && SDL.audio.dest) {
		pauseOn ? SDL.audio.dest.stop() : SDL.audio.dest.start();
      }
      SDL.audio.paused = pauseOn;
    }
});
