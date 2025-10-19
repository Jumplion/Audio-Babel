const express = require('express');
const router = express.Router();
const Wall = require('../models/Wall');

// Get all walls for a specific genre
router.get('/rooms/:genre/walls', async (req, res) => {
    try {
        const genre = req.params.genre;
        const walls = await Wall.find({ genre: genre });
        res.json(walls);
    } catch (error) {
        res.status(500).json({ message: error.message });
    }
});

// Get a specific wall by artist name
router.get('/rooms/:genre/walls/:artist', async (req, res) => {
    try {
        const genre = req.params.genre;
        const artist = req.params.artist;
        const wall = await Wall.findOne({ genre: genre, artist: artist });
        if (!wall) {
            return res.status(404).json({ message: 'Wall not found' });
        }
        res.json(wall);
    } catch (error) {
        res.status(500).json({ message: error.message });
    }
});

// Create a new wall
router.post('/rooms/:genre/walls', async (req, res) => {
    const wall = new Wall({
        genre: req.params.genre,
        artist: req.body.artist,
        shelves: req.body.shelves,
    });

    try {
        const newWall = await wall.save();
        res.status(201).json(newWall);
    } catch (error) {
        res.status(400).json({ message: error.message });
    }
});

// Update a wall by artist name
router.put('/rooms/:genre/walls/:artist', async (req, res) => {
    try {
        const genre = req.params.genre;
        const artist = req.params.artist;
        const wall = await Wall.findOneAndUpdate(
            { genre: genre, artist: artist },
            req.body,
            { new: true }
        );
        if (!wall) {
            return res.status(404).json({ message: 'Wall not found' });
        }
        res.json(wall);
    } catch (error) {
        res.status(400).json({ message: error.message });
    }
});

// Delete a wall by artist name
router.delete('/rooms/:genre/walls/:artist', async (req, res) => {
    try {
        const genre = req.params.genre;
        const artist = req.params.artist;
        const wall = await Wall.findOneAndDelete({ genre: genre, artist: artist });
        if (!wall) {
            return res.status(404).json({ message: 'Wall not found' });
        }
        res.json({ message: 'Wall deleted' });
    } catch (error) {
        res.status(500).json({ message: error.message });
    }
});

module.exports = router;