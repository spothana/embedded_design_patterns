import subprocess
from pathlib import Path

print("="*70)
print("DOCSIS Video Creator (Image + Audio → MP4)")
print("="*70)

# ====================== USER INPUT ======================
BASE_NAME = input("Enter base filename (e.g. DOCSIS4.0_PHY): ").strip() or "DOCSIS4.0_PHY"
NUM_SLIDES = int(input(f"How many slides for {BASE_NAME}? (default 9): ") or "9")

# Folders
OUTPUT_DIR = Path("output")
OUTPUT_DIR.mkdir(exist_ok=True)

RESOLUTION = "3840:2160"
AUDIO_BITRATE = "48k"

def run_ffmpeg(cmd):
    print(f"▶ Running ffmpeg for slide {cmd[cmd.index('-i')+1] if '-i' in cmd else ''}...")    
    print("   Command:", cmd)
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print("❌ Error:", result.stderr[-400:])
        return False
    print("✅ Success")
    return True

# ====================== STEP 1: Individual MP4s ======================
print(f"\n🔨 Creating {NUM_SLIDES} individual MP4s...\n")

individual_mp4s = []

for i in range(1, NUM_SLIDES + 1):
    img = Path(f"{BASE_NAME}{i}.jpg")
    audio = Path(f"{BASE_NAME}{i}.m4a")
    out_mp4 = OUTPUT_DIR / f"{BASE_NAME}{i}.mp4"

    if not img.exists():
        print(f"⚠️ Image not found: {img}")
        continue

    cmd = [
        "ffmpeg", "-y",
        "-hwaccel", "cuda",
        "-loop", "1", "-i", str(img)
    ]

    if audio.exists():
        cmd += ["-i", str(audio)]
    else:
        cmd += ["-f", "lavfi", "-i", "anullsrc=channel_layout=stereo:sample_rate=48000"]

    cmd += [
        "-vf", f"scale={RESOLUTION}:force_original_aspect_ratio=decrease,pad={RESOLUTION}:(ow-iw)/2:(oh-ih)/2,format=yuv420p",
        "-c:v", "h264_nvenc",
        "-c:a", "aac",
        "-b:a", AUDIO_BITRATE,
        "-shortest",
        str(out_mp4)
    ]

    if run_ffmpeg(cmd):
        individual_mp4s.append(out_mp4)

# ====================== STEP 2: Concatenation ======================
print(f"\n🔗 Concatenating {len(individual_mp4s)} slides...")

if len(individual_mp4s) < 1:
    print("❌ No slides created!")
    exit(1)

final_output = OUTPUT_DIR / f"{BASE_NAME}_FINAL.mp4"

# 1. Define the inputs (re-sampling and re-scaling labels)
filter_defs = []
for i in range(len(individual_mp4s)):
    filter_defs.append(f"[{i}:v]setsar=1[v{i}];[{i}:a]aresample=48000[a{i}]")
    

# 2. Join definitions with semicolons
full_filter = ";".join(filter_defs)

# 3. Create the concat chain (Notice the semicolon BEFORE the chain starts)
concat_chain = "".join(f"[v{i}][a{i}]" for i in range(len(individual_mp4s)))
filter_complex = f"{full_filter};{concat_chain}concat=n={len(individual_mp4s)}:v=1:a=1[v][a]"

# Build command
cmd = [
    "ffmpeg", "-y",
]
for mp4 in individual_mp4s:
    cmd.extend(["-i", str(mp4)])

cmd.extend([
    "-filter_complex", filter_complex,
    "-map", "[v]",
    "-map", "[a]",
    "-c:v", "h264_nvenc",
    "-preset", "p6",            # High quality preset
    "-spatial-aq", "1",         # Protects fine text/lines in diagrams
    "-pix_fmt", "yuv420p",
    "-c:a", "aac",
    "-b:a", AUDIO_BITRATE,
    str(final_output)
])

if run_ffmpeg(cmd):
    print("\n" + "="*70)
    print("🎉 SUCCESS! Final video created:")
    print(f"   {final_output.absolute()}")
    print("="*70)
else:
    print("❌ Concatenation failed.")
