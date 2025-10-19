const express = require('express');
const router = express.Router();
const Shelf = require('../models/Shelf');

// Get all shelves for a specific wall (artist)
router.get('/:artistId', async (req, res) => {
    try {
        const shelves = await Shelf.find({ artistId: req.params.artistId });
        res.json(shelves);
    } catch (error) {
        res.status(500).json({ message: error.message });
    }
});

// Get a specific shelf by ID
router.get('/:artistId/:shelfId', async (req, res) => {
    try {
        const shelf = await Shelf.findOne({ 
            artistId: req.params.artistId, 
            _id: req.params.shelfId 
        });
        if (!shelf) {
            return res.status(404).json({ message: 'Shelf not found' });
        }
        res.json(shelf);
    } catch (error) {
        res.status(500).json({ message: error.message });
    }
});

// Create a new shelf
router.post('/', async (req, res) => {
    const shelf = new Shelf({
        artistId: req.body.artistId,
        title: req.body.title,
        tracks: req.body.tracks
    });

    try {
        const newShelf = await shelf.save();
        res.status(201).json(newShelf);
    } catch (error) {
        res.status(400).json({ message: error.message });
    }
});

// Update a shelf
router.patch('/:artistId/:shelfId', async (req, res) => {
    try {
        const shelf = await Shelf.findOneAndUpdate(
            { artistId: req.params.artistId, _id: req.params.shelfId },
            req.body,
            { new: true }
        );
        if (!shelf) {
            return res.status(404).json({ message: 'Shelf not found' });
        }
        res.json(shelf);
    } catch (error) {
        res.status(400).json({ message: error.message });
    }
});

// Delete a shelf
router.delete('/:artistId/:shelfId', async (req, res) => {
    try {
        const shelf = await Shelf.findOneAndDelete({ 
            artistId: req.params.artistId, 
            _id: req.params.shelfId 
        });
        if (!shelf) {
            return res.status(404).json({ message: 'Shelf not found' });
        }
        res.json({ message: 'Shelf deleted' });
    } catch (error) {
        res.status(500).json({ message: error.message });
    }
});

module.exports = router;