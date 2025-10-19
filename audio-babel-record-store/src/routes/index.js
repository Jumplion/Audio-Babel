const express = require('express');
const router = express.Router();

// Import route handlers
const roomsRouter = require('./rooms');
const wallsRouter = require('./walls');
const shelvesRouter = require('./shelves');
const tracksRouter = require('./tracks');

// Define main routes
router.use('/rooms', roomsRouter);
router.use('/walls', wallsRouter);
router.use('/shelves', shelvesRouter);
router.use('/tracks', tracksRouter);

// Export the router
module.exports = router;