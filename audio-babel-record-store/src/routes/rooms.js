const express = require('express');
const router = express.Router();
const Room = require('../models/Room');

// Get all rooms (genres)
router.get('/', async (req, res) => {
    try {
        const rooms = await Room.find();
        res.json(rooms);
    } catch (error) {
        res.status(500).json({ message: error.message });
    }
});

// Get a specific room by genre
router.get('/:genre', async (req, res) => {
    try {
        const room = await Room.findOne({ genre: req.params.genre });
        if (!room) {
            return res.status(404).json({ message: 'Room not found' });
        }
        res.json(room);
    } catch (error) {
        res.status(500).json({ message: error.message });
    }
});

// Create a new room (genre)
router.post('/', async (req, res) => {
    const room = new Room({
        genre: req.body.genre,
        walls: req.body.walls,
    });

    try {
        const newRoom = await room.save();
        res.status(201).json(newRoom);
    } catch (error) {
        res.status(400).json({ message: error.message });
    }
});

// Update a room (genre)
router.patch('/:genre', async (req, res) => {
    try {
        const room = await Room.findOne({ genre: req.params.genre });
        if (!room) {
            return res.status(404).json({ message: 'Room not found' });
        }

        if (req.body.walls) {
            room.walls = req.body.walls;
        }

        const updatedRoom = await room.save();
        res.json(updatedRoom);
    } catch (error) {
        res.status(400).json({ message: error.message });
    }
});

// Delete a room (genre)
router.delete('/:genre', async (req, res) => {
    try {
        const room = await Room.findOne({ genre: req.params.genre });
        if (!room) {
            return res.status(404).json({ message: 'Room not found' });
        }

        await room.remove();
        res.json({ message: 'Room deleted' });
    } catch (error) {
        res.status(500).json({ message: error.message });
    }
});

module.exports = router;