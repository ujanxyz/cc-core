Read the following requirements (rough idea) and convert it into a C++ header.
- Do not generate the implementation code, focus only on creating a well-specified header.
- First propose the high level idea, suggest improvements, edge cases, get feedback from the human.

# Function Store API

This API is an im-memory store of node functions, which can be loaded from a source and looked up.

# Function spec data

A node function is a piece of execution logic that reads from some input params, and produces some output params. The JSON entries below are some example specs, but you will convert these into structs.

```json
[
  {
    id: "/gen/circle",
    label: "Circle Points",
    desc: "Generates points along the circumference of a circle",
    params: [
      { name: "center", access: "i", type: "float2" },
      { name: "radius", access: "i", type: "float" },
      { name: "count", access: "i", type: "int" },
      { name: "points", access: "o", type: "[]float2" },
    ],
  },
  {
    id: "/sampling/color-ramp",
    label: "Color Ramp",
    desc: "Maps a 0.0-1.0 float to a multi-stop color gradient",
    params: [
      { name: "t", access: "i", type: "[]float" },
      { name: "colors", access: "i", type: "[]float3" },
      { name: "stops", access: "i", type: "[]float" },
      { name: "out", access: "o", type: "[]float3" },
    ],
  },
  {
    id: "/export/svg-path",
    label: "SVG Path",
    desc: "Converts a point array into an SVG <path> d-attribute",
    params: [
      { name: "points", access: "i", type: "[]float2" },
      { name: "closed", access: "i", type: "bool" },
      { name: "svgString", access: "o", type: "string" },
    ],
  },
  . . .
]
```

# API methods.

- Add a spec into the store.
- Load a collection of specs from a JSON array (as string).
- Check if a given function id is in the store.
- Lookup a spec by id.
