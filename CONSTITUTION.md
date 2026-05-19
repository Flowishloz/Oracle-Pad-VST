# CONSTITUTION OF THE ORACLE-PAD VST FACTORY

## 1. CONSTRAINTS
- All code must be local. No cloud costs allowed.
- Use only JUCE 8 API for development.
- Do not introduce any hallucinated math.

## 2. BLAURT MATH
Blauert math is a cornerstone of our Psychoacoustic research, ensuring accurate 3D sound mapping and spatial hearing perception. Refer to `/Reference/Spatial_Hearing_Blauert_1997_Revised.pdf` for detailed mathematical formulas and guidelines.

## 3. SNIPE DIRECTIVE
- **MASTER AGENT**: Directs all agents to their specific sub-folders inside `/Reference`.
- **PSYCHOACOUSTIC EXPERT**: Forbidden from guessing formulas. Extract exact coefficients, notch frequencies, and HRTF tables directly from assigned folders.
- **LOFI SAMPLER SPECIALIST**: Use the 1978 Group Delay Distortions paper to model the 'Spread' control. Emulate analog phase anomalies instead of using a modern digital widener.

## 4. DUAL-OSCILLATOR SPLIT
The dual-oscillator split is essential for defining the sonic character of our VST3. Ensure that both oscillators are implemented as separate modules within JUCE 8, with clear differentiation in their signal paths.