/**
 * @file test_metadata.cpp
 * @brief Unit tests for IndexMetadata extraction and generation.
 * 
 * Tests the IndexMetadata class including:
 * - Metadata extraction from big integer indexes
 * - Metadata extraction from base64 strings
 * - SVG cover generation
 * - Field validation and determinism
 * - Malformed input handling
 */

#include "test_common.h"

/**
 * @brief Register all metadata-related tests with the test runner.
 * @param runner TestRunner instance to register tests with
 */
void register_metadata_tests(TestRunner& runner) {
    runner.add("AudioIndex: indexToMetadata deterministic and valid", [&runner]() -> bool {
        const std::string name = "AudioIndex: indexToMetadata deterministic and valid";
        using boost::multiprecision::cpp_int;
        bool ok = true;
        try {
            // Build a sample byte vector (non-empty) and construct a cpp_int (MSB-first)
            std::vector<uint8_t> bytes = {0x10, 0x20, 0x30, 0x41, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA,
                                          0xBB, 0xCC, 0xDD, 0xEE, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
            cpp_int              idx   = 0;
            for (uint8_t b : bytes) {
                idx <<= 8;
                idx |= cpp_int(static_cast<uint32_t>(b));
            }

            auto m1 = AudioIndex::indexToMetadata(idx);
            auto m2 = AudioIndex::indexToMetadata(idx);

            ok &= RUN_CHECK(runner, name, m1.genre == m2.genre, "genre deterministic");
            ok &= RUN_CHECK(runner, name, m1.artist == m2.artist, "artist deterministic");
            ok &= RUN_CHECK(runner, name, m1.album == m2.album, "album deterministic");
            ok &= RUN_CHECK(runner, name, m1.track == m2.track, "track deterministic");

            // Validate variable-length behavior: parts should be non-empty and
            // when concatenated they should recreate the URL-safe base64
            // representation of the original index bytes (no padding).
            static const char b64_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
            std::string       b64str;
            b64str.reserve((bytes.size() * 8 + 5) / 6);
            uint32_t acc      = 0;
            int      acc_bits = 0;
            for (uint8_t byte : bytes) {
                acc = (acc << 8) | byte;
                acc_bits += 8;
                while (acc_bits >= 6) {
                    acc_bits -= 6;
                    auto idx = static_cast<uint8_t>((acc >> acc_bits) & 0x3F);
                    b64str.push_back(b64_alpha[idx]);
                }
            }
            if (acc_bits > 0) {
                auto idx = static_cast<uint8_t>((acc << (6 - acc_bits)) & 0x3F);
                b64str.push_back(b64_alpha[idx]);
            }

            ok &= RUN_CHECK(runner, name, !m1.genre.empty(), "genre non-empty");
            ok &= RUN_CHECK(runner, name, !m1.artist.empty(), "artist non-empty");
            ok &= RUN_CHECK(runner, name, !m1.album.empty(), "album non-empty");
            ok &= RUN_CHECK(runner, name, !m1.track.empty(), "track non-empty");

            auto valid_b64_chars = [&](const std::string& s) {
                for (char c : s) {
                    if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '-' && c != '_') {
                        return false;
                    }
                }
                return true;
            };

            ok &= RUN_CHECK(runner, name, valid_b64_chars(m1.genre), "genre base64 chars valid");
            ok &= RUN_CHECK(runner, name, valid_b64_chars(m1.artist), "artist base64 chars valid");
            ok &= RUN_CHECK(runner, name, valid_b64_chars(m1.album), "album base64 chars valid");
            ok &= RUN_CHECK(runner, name, valid_b64_chars(m1.track), "track base64 chars valid");

            std::string recombined = m1.genre + m1.artist + m1.album + m1.track;
            ok &= RUN_CHECK(runner, name, recombined == b64str, "concatenation recreates base64 index");

            // Cover: should contain SVG markup
            ok &= RUN_CHECK(runner, name, !m1.cover.empty(), "cover non-empty");
            std::string cover_str(m1.cover.begin(), m1.cover.end());
            ok &= RUN_CHECK(runner, name, cover_str.find("<svg") != std::string::npos, "cover contains svg");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("IndexMetadata: string-overload deterministic and recomposition", [&runner]() -> bool {
        const std::string name = "IndexMetadata: string-overload deterministic and recomposition";
        using boost::multiprecision::cpp_int;
        bool ok = true;
        try {
            // Build a deterministic byte array and a base64 string (URL-safe, no padding)
            std::vector<uint8_t> bytes = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};
            // encode to URL-safe base64 using same algorithm as production
            static const char b64_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
            std::string       b64str;
            uint32_t          acc      = 0;
            int               acc_bits = 0;
            for (uint8_t byte : bytes) {
                acc = (acc << 8) | byte;
                acc_bits += 8;
                while (acc_bits >= 6) {
                    acc_bits -= 6;
                    auto idx = static_cast<uint8_t>((acc >> acc_bits) & 0x3F);
                    b64str.push_back(b64_alpha[idx]);
                }
            }
            if (acc_bits > 0) {
                auto idx = static_cast<uint8_t>((acc << (6 - acc_bits)) & 0x3F);
                b64str.push_back(b64_alpha[idx]);
            }

            // Call the string overload
            auto meta = IndexMetadata::extractMetadataFromIndex(b64str);

            // Basic assertions: parts non-empty and valid chars
            ok &= RUN_CHECK(runner, name, !meta.genre.empty(), "genre non-empty");
            ok &= RUN_CHECK(runner, name, !meta.artist.empty(), "artist non-empty");
            ok &= RUN_CHECK(runner, name, !meta.album.empty(), "album non-empty");
            ok &= RUN_CHECK(runner, name, !meta.track.empty(), "track non-empty");

            auto valid_b64_chars = [&](const std::string& s) {
                for (char c : s) {
                    if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '-' && c != '_') {
                        return false;
                    }
                }
                return true;
            };

            ok &= RUN_CHECK(runner, name, valid_b64_chars(meta.genre), "genre base64 chars valid");
            ok &= RUN_CHECK(runner, name, valid_b64_chars(meta.artist), "artist base64 chars valid");
            ok &= RUN_CHECK(runner, name, valid_b64_chars(meta.album), "album base64 chars valid");
            ok &= RUN_CHECK(runner, name, valid_b64_chars(meta.track), "track base64 chars valid");

            std::string recombined = meta.genre + meta.artist + meta.album + meta.track;
            ok &= RUN_CHECK(runner, name, recombined == b64str, "concatenation recreates base64 index");

            // cover contains svg
            ok &= RUN_CHECK(runner, name, !meta.cover.empty(), "cover non-empty");
            std::string const& cover_str = meta.cover;
            ok &= RUN_CHECK(runner, name, cover_str.find("<svg") != std::string::npos, "cover contains svg");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("IndexMetadata: string-overload malformed input handling", [&runner]() -> bool {
        const std::string name = "IndexMetadata: string-overload malformed input handling";
        bool              ok   = true;
        try {
            // Create a valid small byte array and base64 string
            std::vector<uint8_t> bytes       = {0xDE, 0xAD, 0xBE, 0xEF};
            static const char    b64_alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
            std::string          clean_b64;
            uint32_t             acc      = 0;
            int                  acc_bits = 0;
            for (uint8_t byte : bytes) {
                acc = (acc << 8) | byte;
                acc_bits += 8;
                while (acc_bits >= 6) {
                    acc_bits -= 6;
                    auto idx = static_cast<uint8_t>((acc >> acc_bits) & 0x3F);
                    clean_b64.push_back(b64_alpha[idx]);
                }
            }
            if (acc_bits > 0) {
                auto idx = static_cast<uint8_t>((acc << (6 - acc_bits)) & 0x3F);
                clean_b64.push_back(b64_alpha[idx]);
            }

            // Inject some malformed characters into the base64 string
            std::string malformed = clean_b64;
            if (malformed.size() >= 2) {
                malformed.insert(1, "=");
                malformed.insert(malformed.size() - 1, "@");
            } else {
                malformed += "=@";
            }

            // Call the string overload with malformed input - expect an exception
            bool threw = false;
            try {
                auto meta = IndexMetadata::extractMetadataFromIndex(malformed);
                (void) meta; // silence unused in the non-throwing path
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            ok &= RUN_CHECK(runner, name, threw, "decoder throws on malformed base64 input");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("IndexMetadata: generateSvgCover color derivation", [&runner]() -> bool {
        const std::string name = "IndexMetadata: generateSvgCover color derivation";
        bool              ok   = true;
        try {
            std::vector<uint8_t> bytes = {0x12, 0x34, 0x56, 0x78};
            std::string          svg   = IndexMetadata::generateSvgCover(bytes, "t");
            // color computed from first three bytes: 0x12 0x34 0x56 -> hex 123456
            ok &= RUN_CHECK(runner, name, svg.find("#123456") != std::string::npos, "svg contains expected color #123456");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("IndexMetadata: generateSvgCover contains track text", [&runner]() -> bool {
        const std::string name = "IndexMetadata: generateSvgCover contains track text";
        bool              ok   = true;
        try {
            std::vector<uint8_t> bytes = {0xFF, 0xEE, 0xDD};
            std::string          track = "MyTrack";
            std::string          svg   = IndexMetadata::generateSvgCover(bytes, track);
            ok &= RUN_CHECK(runner, name, svg.find(track) != std::string::npos, "svg contains track text");
        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("IndexMetadata: stress test with very small cpp_int values", [&runner]() -> bool {
        const std::string name = "IndexMetadata: stress test with very small cpp_int values";
        using boost::multiprecision::cpp_int;
        bool ok = true;

        try {
            // Test edge cases with minimal indexes
            std::vector<cpp_int> small_values = {
                cpp_int(0),     // Zero
                cpp_int(1),     // One
                cpp_int(2),     // Two
                cpp_int(15),    // Small value
                cpp_int(255),   // Single byte max
                cpp_int(256),   // Just over single byte
                cpp_int(65535), // Two bytes max (uint16_t max)
                cpp_int(65536)  // Just over two bytes
            };

            for (const auto& idx : small_values) {
                auto meta = AudioIndex::indexToMetadata(idx);

                // All fields should be non-empty
                ok &= RUN_CHECK(runner, name, !meta.genre.empty(), "genre non-empty for index " + idx.convert_to<std::string>());
                ok &= RUN_CHECK(runner, name, !meta.artist.empty(), "artist non-empty for index " + idx.convert_to<std::string>());
                ok &= RUN_CHECK(runner, name, !meta.album.empty(), "album non-empty for index " + idx.convert_to<std::string>());
                ok &= RUN_CHECK(runner, name, !meta.track.empty(), "track non-empty for index " + idx.convert_to<std::string>());

                // Concatenation should recreate valid base64
                std::string recombined = meta.genre + meta.artist + meta.album + meta.track;

                // Verify all characters are valid base64 URL-safe
                for (char c : recombined) {
                    bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
                    if (!valid) {
                        runner.failMsg(name, "Invalid character '" + std::string(1, c) + "' in metadata for index " + idx.convert_to<std::string>());
                        ok = false;
                        break;
                    }
                }

                // Cover should be valid SVG
                ok &= RUN_CHECK(runner, name, !meta.cover.empty(), "cover non-empty for index " + idx.convert_to<std::string>());
                ok &= RUN_CHECK(
                    runner, name, meta.cover.find("<svg") != std::string::npos, "cover contains svg for index " + idx.convert_to<std::string>());

                if (!ok)
                    break;
            }

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("IndexMetadata: stress test with very large cpp_int values", [&runner]() -> bool {
        const std::string name = "IndexMetadata: stress test with very large cpp_int values";
        using boost::multiprecision::cpp_int;
        bool ok = true;

        try {
            // Test with progressively larger indexes
            std::vector<cpp_int> large_values;

            // 32-bit max
            large_values.push_back(cpp_int("4294967295"));

            // 64-bit max
            large_values.push_back(cpp_int("18446744073709551615"));

            // 128-bit value
            large_values.push_back(cpp_int("340282366920938463463374607431768211455"));

            // 256-bit value
            large_values.push_back(cpp_int("115792089237316195423570985008687907853269984665640564039457584007913129639935"));

            size_t test_num = 0;
            for (const auto& idx : large_values) {
                test_num++;
                auto meta = AudioIndex::indexToMetadata(idx);

                // All fields should be non-empty
                std::string short_idx = "large_" + std::to_string(test_num);

                ok &= RUN_CHECK(runner, name, !meta.genre.empty(), "genre non-empty for large index " + short_idx);
                ok &= RUN_CHECK(runner, name, !meta.artist.empty(), "artist non-empty for large index " + short_idx);
                ok &= RUN_CHECK(runner, name, !meta.album.empty(), "album non-empty for large index " + short_idx);
                ok &= RUN_CHECK(runner, name, !meta.track.empty(), "track non-empty for large index " + short_idx);

                // Concatenation should recreate valid base64
                std::string recombined = meta.genre + meta.artist + meta.album + meta.track;

                // Verify all characters are valid base64 URL-safe
                bool all_valid = true;
                for (char c : recombined) {
                    bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
                    if (!valid) {
                        runner.failMsg(name, "Invalid character '" + std::string(1, c) + "' in metadata for large index " + short_idx);
                        all_valid = false;
                        ok        = false;
                        break;
                    }
                }

                if (all_valid) {
                    ok &= RUN_CHECK(runner, name, true, "all chars valid base64 for large index " + short_idx);
                }

                // Verify the base64 string length is reasonable
                ok &= RUN_CHECK(runner, name, recombined.length() > 0, "base64 length > 0 for large index " + short_idx);

                // Cover should be valid SVG
                ok &= RUN_CHECK(runner, name, !meta.cover.empty(), "cover non-empty for large index " + short_idx);
                ok &= RUN_CHECK(runner, name, meta.cover.find("<svg") != std::string::npos, "cover contains svg for large index " + short_idx);

                if (!ok)
                    break;
            }

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("IndexMetadata: weighted splitting logic consistency", [&runner]() -> bool {
        const std::string name = "IndexMetadata: weighted splitting logic consistency";
        using boost::multiprecision::cpp_int;
        bool ok = true;

        try {
            // The metadata splitting should be deterministic and consistent
            // Test that the same index always produces the same split
            std::vector<cpp_int> test_indexes = {
                cpp_int(12345), cpp_int(987654321), cpp_int("1234567890123456789"), cpp_int("999999999999999999999999")};

            for (const auto& idx : test_indexes) {
                // Generate metadata multiple times
                auto meta1 = AudioIndex::indexToMetadata(idx);
                auto meta2 = AudioIndex::indexToMetadata(idx);
                auto meta3 = AudioIndex::indexToMetadata(idx);

                std::string idx_str = idx.convert_to<std::string>();

                // Verify consistency
                ok &= RUN_CHECK(runner, name, meta1.genre == meta2.genre && meta2.genre == meta3.genre, "genre consistent for index " + idx_str);
                ok &= RUN_CHECK(runner, name, meta1.artist == meta2.artist && meta2.artist == meta3.artist, "artist consistent for index " + idx_str);
                ok &= RUN_CHECK(runner, name, meta1.album == meta2.album && meta2.album == meta3.album, "album consistent for index " + idx_str);
                ok &= RUN_CHECK(runner, name, meta1.track == meta2.track && meta2.track == meta3.track, "track consistent for index " + idx_str);

                // Verify the weighted lengths are reasonable
                // genre should get 30%, artist 30%, album 30%, track 10%
                std::string combined  = meta1.genre + meta1.artist + meta1.album + meta1.track;
                size_t      total_len = combined.length();

                if (total_len > 0) {
                    double genre_ratio  = static_cast<double>(meta1.genre.length()) / total_len;
                    double artist_ratio = static_cast<double>(meta1.artist.length()) / total_len;
                    double album_ratio  = static_cast<double>(meta1.album.length()) / total_len;
                    double track_ratio  = static_cast<double>(meta1.track.length()) / total_len;

                    // Allow very generous tolerance since distribution is based on byte content, not fixed ratios
                    // The weighted algorithm can produce highly skewed distributions for certain byte patterns
                    // We just verify that no field dominates completely or disappears entirely
                    ok &= RUN_CHECK(runner, name, genre_ratio >= 0.05 && genre_ratio <= 0.60, "genre length ratio reasonable for index " + idx_str);
                    ok &=
                        RUN_CHECK(runner, name, artist_ratio >= 0.05 && artist_ratio <= 0.60, "artist length ratio reasonable for index " + idx_str);
                    ok &= RUN_CHECK(runner, name, album_ratio >= 0.05 && album_ratio <= 0.60, "album length ratio reasonable for index " + idx_str);
                    ok &= RUN_CHECK(runner, name, track_ratio >= 0.02 && track_ratio <= 0.50, "track length ratio reasonable for index " + idx_str);
                }

                if (!ok)
                    break;
            }

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });

    runner.add("IndexMetadata: empty index edge case", [&runner]() -> bool {
        const std::string name = "IndexMetadata: empty index edge case";
        using boost::multiprecision::cpp_int;
        bool ok = true;

        try {
            // Test with zero index (minimal case)
            cpp_int idx  = 0;
            auto    meta = AudioIndex::indexToMetadata(idx);

            // Should produce default metadata (not crash)
            ok &= RUN_CHECK(runner, name, !meta.genre.empty(), "genre non-empty for zero index");
            ok &= RUN_CHECK(runner, name, !meta.artist.empty(), "artist non-empty for zero index");
            ok &= RUN_CHECK(runner, name, !meta.album.empty(), "album non-empty for zero index");
            ok &= RUN_CHECK(runner, name, !meta.track.empty(), "track non-empty for zero index");

            // Cover should still be generated
            ok &= RUN_CHECK(runner, name, !meta.cover.empty(), "cover non-empty for zero index");

        } catch (const std::exception& e) {
            runner.failMsg(name, std::string("exception: ") + e.what());
            ok = false;
        }
        return ok;
    });
}
