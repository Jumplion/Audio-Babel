const express = require('express');
const router = express.Router();
const Track = require('../models/Track');

// Get all tracks
router.get('/', async (req, res) => {
    try {
        const tracks = await Track.find();
        res.json(tracks);
    } catch (error) {
        res.status(500).json({ message: error.message });
    }
});

// Get a specific track by ID
router.get('/:id', async (req, res) => {
    try {
        const track = await Track.findById(req.params.id);
        if (!track) {
            return res.status(404).json({ message: 'Track not found' });
        }
        res.json(track);
    } catch (error) {
        res.status(500).json({ message: error.message });
    }
});

// Create a new track
router.post('/', async (req, res) => {
    const track = new Track({
        title: req.body.title,
        artist: req.body.artist,
        album: req.body.album,
        genre: req.body.genre,
        duration: req.body.duration,
        index: req.body.index
    });

    try {
        const newTrack = await track.save();
        res.status(201).json(newTrack);
    } catch (error) {
        res.status(400).json({ message: error.message });
    }
});

// Update a track
router.patch('/:id', async (req, res) => {
    try {
        const track = await Track.findById(req.params.id);
        if (!track) {
            return res.status(404).json({ message: 'Track not found' });
        }

        if (req.body.title) track.title = req.body.title;
        if (req.body.artist) track.artist = req.body.artist;
        if (req.body.album) track.album = req.body.album;
        if (req.body.genre) track.genre = req.body.genre;
        if (req.body.duration) track.duration = req.body.duration;
        if (req.body.index) track.index = req.body.index;

        const updatedTrack = await track.save();
        res.json(updatedTrack);
    } catch (error) {
        res.status(400).json({ message: error.message });
    }
});

// Delete a track
router.delete('/:id', async (req, res) => {
    try {
        const track = await Track.findById(req.params.id);
        if (!track) {
            return res.status(404).json({ message: 'Track not found' });
        }

        await track.remove();
        res.json({ message: 'Track deleted' });
    } catch (error) {
        res.status(500).json({ message: error.message });
    }
});

module.exports = router;