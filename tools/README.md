# Map asset tooling

`build_map_assets.py` converts public domain shapefiles into the binary
layers the app loads from `assets/maps/` at startup. Run it by hand when
refreshing the committed assets, it is not part of the build.

Sources (all public domain):

- States: US Census cartographic boundary file, 1:5,000,000
  https://www2.census.gov/geo/tiger/GENZ2023/shp/cb_2023_us_state_5m.zip
- Counties: US Census cartographic boundary file, 1:5,000,000
  https://www2.census.gov/geo/tiger/GENZ2023/shp/cb_2023_us_county_5m.zip
- Populated places: Natural Earth 1:10m populated places (simple)
  https://naciscdn.org/naturalearth/10m/cultural/ne_10m_populated_places_simple.zip

Regenerating:

```sh
pip3 install pyshp
python3 tools/build_map_assets.py \
    --states   path/to/cb_2023_us_state_5m.shp \
    --counties path/to/cb_2023_us_county_5m.shp \
    --places   path/to/ne_10m_populated_places_simple.shp \
    --out assets/maps
```

County rings get Douglas-Peucker simplified (default ~300m tolerance) to keep
the committed file under ~1MB. The binary formats are described in the script
header and parsed by `src/app/map_data.cpp`.
