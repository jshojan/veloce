#pragma once
#include "emu/tas_plugin.hpp"
namespace emu::tas {
enum class MovieFormat { TAS1, TAS2, FM2 };
struct Movie {
    TASMovieInfo info{};
    std::vector<uint8_t> start_state;
    std::vector<TASFrameData> frames;
    MovieFormat format = MovieFormat::TAS2;
};
// Transactional read: on failure the destination is unchanged. TAS2 writes
// explicit little-endian fields; legacy TAS1 imports use this build's layout.
bool load_movie_file(const char* filename, Movie& movie);
bool save_movie_file(const char* filename, const Movie& movie);
}
