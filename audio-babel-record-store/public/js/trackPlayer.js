// trackPlayer.js

document.addEventListener('DOMContentLoaded', function() {
    const audioElement = document.getElementById('audio-player');
    const playButton = document.getElementById('play-button');
    const pauseButton = document.getElementById('pause-button');
    const stopButton = document.getElementById('stop-button');
    const trackTitle = document.getElementById('track-title');

    let currentTrack = null;

    function loadTrack(track) {
        if (currentTrack !== track) {
            audioElement.src = track.audioUrl;
            trackTitle.textContent = track.title;
            currentTrack = track;
        }
    }

    function playTrack() {
        if (currentTrack) {
            audioElement.play();
        }
    }

    function pauseTrack() {
        audioElement.pause();
    }

    function stopTrack() {
        audioElement.pause();
        audioElement.currentTime = 0;
    }

    playButton.addEventListener('click', playTrack);
    pauseButton.addEventListener('click', pauseTrack);
    stopButton.addEventListener('click', stopTrack);

    // Example of loading a track (this should be replaced with actual track data)
    const exampleTrack = {
        title: 'Example Track',
        audioUrl: 'path/to/example-track.mp3'
    };
    loadTrack(exampleTrack);
});