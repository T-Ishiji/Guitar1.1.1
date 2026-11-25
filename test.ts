// v1.1.1: 8ボタン状態でC4～C5を演奏する例。
// 音色はDoublePulse / TriplePulseも選択可能。金管モードは従来の25%固定。
guitarchord.setPlayMode(guitarchord.PlayMode.Harmonics2)
guitarchord.setTimbre(guitarchord.Timbre.DoublePulse)
guitarchord.setDelay(0)
guitarchord.assignChord(guitarchord.ChordButton.NoButton, guitarchord.Chord.C)
guitarchord.assignChord(guitarchord.ChordButton.Button1, guitarchord.Chord.D)
guitarchord.assignChord(guitarchord.ChordButton.Button2, guitarchord.Chord.E)
guitarchord.assignChord(guitarchord.ChordButton.Button1Button2, guitarchord.Chord.F)
guitarchord.assignChord(guitarchord.ChordButton.Button3, guitarchord.Chord.G)
guitarchord.assignChord(guitarchord.ChordButton.Button1Button3, guitarchord.Chord.A)
guitarchord.assignChord(guitarchord.ChordButton.Button2Button3, guitarchord.Chord.B)
guitarchord.assignChord(guitarchord.ChordButton.Button1Button2Button3, guitarchord.Chord.C5)
