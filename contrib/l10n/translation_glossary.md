# Deluge firmware UI translation — rules and glossary

## Context
You are translating UI strings for the Synthstrom Deluge, a hardware sequencer/synthesizer/sampler.
Strings appear on a small 128x48 OLED screen. Users are musicians familiar with standard
music-production vocabulary in their language. Brevity and consistency matter more than literary polish.

## HARD RULES (violations break the build or the display — check every string)
1. **Printable ASCII only** (0x20–0x7E). The display font cannot render anything else.
   Drop diacritics: é→e, è→e, ê→e, à→a, â→a, ù→u, û→u, î→i, ï→i, ô→o, ç→c, í→i, ó→o, ú→u, á→a, ü→u, œ→oe.
2. **Spanish ñ is forbidden.** Do NOT just strip the tilde when that produces a wrong or crude word
   (año→"ano" is FORBIDDEN). Use a different word instead: añadir→agregar, tamaño→dimension/largo,
   pequeño→menor/chico, señal→(rephrase, e.g. "entrada" or "audio"), enseñar→mostrar, diseño→formato.
   Only strip the tilde when the result is a harmless, recognizable word (e.g. cabaña is never needed anyway).
3. **No double-quote `"` and no backslash `\`** anywhere in a translation. Apostrophes `'` are fine.
4. **Concise**: aim for at most the English length + 25%. Menu/parameter names ideally <= 20 chars;
   messages <= 45 chars. Abbreviate rather than overflow (e.g. FR "Enreg." for "Enregistrement" where cramped).
5. Values must be non-empty. Keys must be copied exactly, unchanged.
6. Match the English capitalization pattern: Title Case for menu/parameter names ("Master Level"),
   Sentence case for messages ("Card is full").
7. **If the right UI text is identical to the English, OMIT the key entirely** — the firmware falls
   back to English automatically. (Examples that should usually be omitted: "MIDI", "Tempo", "Clip",
   "Pan", "Sync", "Mono", "Stereo", "Volume" (ES), "Delay", "Reverb", "LFO 1", note names, numbers.)
8. Never translate these terms (keep verbatim inside sentences): MIDI, USB, CV, SD, OLED, LFO,
   LPF, HPF, EQ, FM, MPE, PPQN, DX7, sysex, arp (as abbreviation), swing, Deluge, mod matrix names
   like "Env 1", waveform "wavetable" position names, "Gate" (CV/trigger context), note names (C, C#, Db...).
9. Deluge product concepts keep their English names in both languages: **Clip, Kit, Pad, Sample,
   Preset, Patch, Slot, Drum (a kit voice), Fill, Overdub, Sidechain, Wavetable, Time-stretch (FR),
   MIDI Learn, MIDI Follow, Takeover, Stem, Sinte(ES synth)** — see tables.
10. Musical/technical loanwords that stay English in music-production usage stay English
    (Feedback, Attack in ES? NO — see tables; follow the tables, they win over your instinct).

## French terminology (use these consistently)
Song=Morceau; Track=Piste; Row=Rangee; NoteRow=Rangee de notes; Section=Section; Grid=Grille;
Session=Session; Arrangement=Arrangement; Arranger (view)=Arrangeur; Keyboard=Clavier; Pad=Pad;
Button=Bouton; Encoder=Encodeur; Knob=Encodeur; Shortcut=Raccourci; Settings=Reglages;
Defaults=Par defaut; Community features=Fonctions communautaires; Firmware=Firmware;
Sound=Son; Synth=Synthe; Instrument=Instrument; Audio track=Piste audio; Output=Sortie; Input=Entree;
Playback=Lecture; Play=Lecture; Record/Recording=Enregistrement; Resample=Reechantillonner;
Recorder=Enregistreur; Loop=Boucle; Loop points=Points de boucle; Layering=Superposition;
Save=Sauvegarder; Load=Charger; Loading=Chargement; Saving=Sauvegarde; Delete=Supprimer;
Clear=Effacer; Copy=Copier; Paste=Coller; Rename=Renommer; New=Nouveau; Empty=Vide; None=Aucun;
All=Tous; Enabled=Active; Disabled=Desactive; Error=Erreur; Can't=Impossible de; not found=introuvable;
SD card=Carte SD; File=Fichier; Folder=Dossier; Battery=Batterie; Brightness=Luminosite;
Level=Niveau; Master level=Niveau general; Width=Largeur; Threshold=Seuil; Compressor=Compresseur;
Metronome=Metronome; Count-in=Decompte; Bar=Mesure; Beat=Temps; Step=Pas; Note=Note;
Velocity=Velocite; Probability=Probabilite; Quantize=Quantiser; Nudge=Decaler; Undo=Annuler;
Redo=Retablir; Filter=Filtre; Cutoff=Coupure; Resonance=Resonance; Envelope=Enveloppe;
Attack=Attaque; Decay=Declin; Sustain=Maintien; Release=Relachement; Rate=Vitesse; Speed=Vitesse;
Depth=Profondeur; Amount=Quantite; Feedback=Feedback; Distortion=Distorsion; Pitch=Pitch;
Transpose=Transposition/Transposer; Tune=Accorder; Detune=Desaccord; Scale=Gamme;
Scale mode=Mode gamme; Chord=Accord; Root note=Fondamentale; Key=Tonalite; Octave=Octave;
Semitone=Demi-ton; Cents=Cents; Oscillator=Oscillateur (abbrev Osc); Noise=Bruit; Unison=Unisson;
Polyphony=Polyphonie; Voice=Voix; Priority=Priorite; Portamento=Portamento; Waveform=Forme d'onde;
Sine=Sinus; Triangle=Triangle; Square=Carre; Saw=Scie; Pulse width=Largeur d'impulsion (abbrev Larg. impulsion);
Sample start/end=Debut/Fin du sample; Reverse=Inverse; Stretch=Etirement; Interpolation=Interpolation;
Linear=Lineaire; Channel=Canal; Device=Appareil; Command=Commande; Select=Selectionner;
Press=Appuyer; Hold=Maintenir; Turn=Tourner; Mode=Mode; Type=Type; Range=Plage;
Bend range=Plage de bend; Zone=Zone; Waiting=Attente; Working=Traitement; Done=Termine;
Overwrite=Ecraser; Confirm=Confirmer; Yes=Oui; No=Non; Sure?=Confirmer ?; Export=Exporter;
Stem export=Export de stems; Mix=Mix; Send=Envoi; Pan=Pan (omit); Version=Version (omit)

## Spanish terminology (use these consistently)
Song=Cancion; Track=Pista; Row=Fila; NoteRow=Fila de notas; Section=Seccion; Grid=Cuadricula;
Session=Sesion; Arrangement=Arreglo; Arranger (view)=Arreglador; Keyboard=Teclado; Pad=Pad;
Button=Boton; Encoder=Encoder; Shortcut=Atajo; Settings=Ajustes; Defaults=Por defecto;
Community features=Funciones de comunidad; Firmware=Firmware; Sound=Sonido; Synth=Sinte;
Instrument=Instrumento; Audio track=Pista de audio; Output=Salida; Input=Entrada;
Playback=Reproduccion; Play=Reproducir; Record/Recording=Grabacion; (to) record=grabar;
Resample=Remuestrear; Recorder=Grabadora; Loop=Bucle; Loop points=Puntos de bucle;
Layering=Capas; Save=Guardar; Load=Cargar; Loading=Cargando; Saving=Guardando;
Delete=Eliminar; Clear=Borrar; Copy=Copiar; Paste=Pegar; Rename=Renombrar; New=Nuevo;
Empty=Vacio; None=Ninguno; All=Todos; Enabled=Activado; Disabled=Desactivado; Error=Error (omit);
Can't=No se puede; not found=no encontrado; SD card=Tarjeta SD; File=Archivo; Folder=Carpeta;
Battery=Bateria; Brightness=Brillo; Level=Nivel; Master level=Nivel general; Width=Ancho;
Threshold=Umbral; Compressor=Compresor; Metronome=Metronomo; Count-in=Cuenta previa;
Bar=Compas; Beat=Pulso; Step=Paso; Note=Nota; Velocity=Velocidad; Probability=Probabilidad;
Quantize=Cuantizar; Nudge=Desplazar; Undo=Deshacer; Redo=Rehacer; Filter=Filtro; Cutoff=Corte;
Resonance=Resonancia; Envelope=Envolvente; Attack=Ataque; Decay=Decaimiento;
Sustain=Sustain (NEVER "Sostenido" — that means the sharp sign); Release=Liberacion;
Sharp=Sostenido; Flat=Bemol; Rate=Velocidad; Speed=Velocidad; Depth=Profundidad;
Amount=Cantidad; Feedback=Feedback; Distortion=Distorsion; Pitch=Pitch;
Transpose=Transposicion/Transponer; Tune=Afinar; Detune=Desafinacion; Scale=Escala;
Scale mode=Modo escala; Chord=Acorde; Root note=Raiz; Key=Tonalidad; Octave=Octava;
Semitone=Semitono; Cents=Cents; Oscillator=Oscilador (abbrev Osc); Noise=Ruido; Unison=Unisono;
Polyphony=Polifonia; Voice=Voz; Priority=Prioridad; Portamento=Portamento; Waveform=Forma de onda;
Sine=Seno; Triangle=Triangulo; Square=Cuadrada; Saw=Sierra; Pulse width=Ancho de pulso;
Sample start/end=Inicio/Fin del sample; Reverse=Invertir/Invertido; Stretch=Estirar;
Interpolation=Interpolacion; Linear=Lineal; Channel=Canal; Device=Dispositivo; Command=Comando;
Select=Seleccionar; Press=Pulsar; Hold=Mantener; Turn=Girar; Mode=Modo; Type=Tipo; Range=Rango;
Bend range=Rango de bend; Zone=Zona; Waiting=Esperando; Working=Procesando; Done=Listo;
Overwrite=Sobrescribir; Confirm=Confirmar; Yes=Si; No=No; Export=Exportar;
Stem export=Exportar stems; Mix=Mezcla; Send=Envio; Version=Version (omit)

## Output format
Write ONE strict JSON object: `{ "KEY": "translation", ... }` — same keys as the input chunk,
minus any keys you deliberately omit (rule 7). No comments, no trailing commas, no markdown fences.

## German terminology (use these consistently)
Song=Song; Track=Spur; Row=Reihe; NoteRow=Notenreihe; Section=Sektion; Grid=Raster;
Session=Session; Arrangement=Arrangement; Arranger (view)=Arranger; Keyboard=Keyboard; Pad=Pad;
Button=Taste; Encoder=Encoder; Shortcut=Shortcut; Settings=Einstellungen (title-abbrev: Einstell.);
Defaults=Standards; Community features=Community-Funktionen (abbrev: Comm.-Funkt.); Firmware=Firmware;
Sound=Sound; Synth=Synth; Instrument=Instrument; Audio track=Audiospur; Output=Ausgang; Input=Eingang;
Playback=Wiedergabe; Play=Wiedergabe; Record/Recording=Aufnahme; (to) record=aufnehmen;
Resample=Resampeln; Recorder=Rekorder; Loop=Loop; Loop points=Loop-Punkte; Layering=Layering;
Save=Speichern; Load=Laden; Loading=Laden...; Saving=Speichern...; Delete=Loeschen; Clear=Leeren;
Copy=Kopieren; Paste=Einfuegen; Rename=Umbenennen; New=Neu; Empty=Leer; None=Keine; All=Alle;
Enabled=An; Disabled=Aus; Error=Fehler; Can't=nicht moeglich; not found=nicht gefunden;
SD card=SD-Karte; File=Datei; Folder=Ordner; Battery=Batterie; Brightness=Helligkeit;
Level=Pegel; Master level=Gesamtpegel; Width=Breite; Threshold=Schwelle; Compressor=Kompressor;
Metronome=Metronom; Count-in=Vorzaehler; Bar=Takt; Beat=Schlag; Step=Schritt; Note=Note;
Velocity=Velocity; Probability=Wahrscheinlichkeit (abbrev: Wahrsch.); Quantize=Quantisieren;
Nudge=Verschieben; Undo=Rueckgaengig; Redo=Wiederholen; Filter=Filter; Cutoff=Cutoff;
Resonance=Resonanz; Envelope=Huellkurve; Attack=Attack; Decay=Decay; Sustain=Sustain;
Release=Release (ADSR terms stay English, as on German synth hardware); Sharp=Kreuz (#); Flat=b;
Rate=Rate; Speed=Tempo/Geschwindigkeit (abbrev: Geschw.); Depth=Tiefe; Amount=Anteil;
Feedback=Feedback; Distortion=Verzerrung; Pitch=Pitch; Transpose=Transponieren/Transposition;
Tune=Stimmen; Detune=Verstimmung; Scale=Skala; Scale mode=Skalenmodus; Chord=Akkord;
Root note=Grundton; Key=Tonart; Octave=Oktave; Semitone=Halbton; Cents=Cents;
Oscillator=Oszillator (abbrev Osz); Noise=Rauschen; Unison=Unisono; Polyphony=Polyphonie;
Voice=Stimme; Priority=Prioritaet; Portamento=Portamento; Waveform=Wellenform; Sine=Sinus;
Triangle=Dreieck; Square=Rechteck; Saw=Saegezahn; Pulse width=Pulsbreite;
Sample start/end=Sample-Start/Ende; Reverse=Rueckwaerts; Stretch=Dehnen; Interpolation=Interpolation;
Linear=Linear; Channel=Kanal; Device=Geraet; Command=Befehl; Select=Waehlen; Press=Druecken;
Hold=Halten; Turn=Drehen; Mode=Modus; Type=Typ; Range=Bereich; Bend range=Bend-Bereich;
Zone=Zone; Waiting=Warten; Working=Verarbeite; Done=Fertig; Overwrite=Ueberschreiben;
Confirm=Bestaetigen; Yes=Ja; No=Nein; Export=Exportieren (abbrev: Export); Mix=Mix; Send=Send

### German umlaut policy (rule 1 addendum)
Never drop the e: ä→ae, ö→oe, ü→ue, ß→ss (Ae/Oe/Ue at word start). "Länge"→"Laenge", "Größe"→"Groesse".

### German length discipline (CRITICAL — German runs long, the display does not)
German compounds overflow the 128px title area easily. Prefer short forms and accepted
abbreviations (Einst., Geschw., Wahrsch., Lautst., Anz., Std.) over full compounds.
Two extra key lists apply to YOUR chunk (check every key against them):
- ctx_keys.txt: value must be <= 15 characters TOTAL. No exceptions.
- title_keys.txt: value must be <= 15 characters, or <= 16 if it contains several narrow
  letters (i, l, t, j, f) or periods. When in doubt, abbreviate harder.
Keys in neither list: follow rule 4 (<= English + 25%, no word over 19 chars).
