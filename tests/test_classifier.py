from disk_cleaner.classifier import FileCategory, classify


def test_video_extensions() -> None:
    for name in ("movie.mp4", "clip.MKV", "scene.WebM"):
        assert classify(name) is FileCategory.VIDEO


def test_photo_extensions() -> None:
    for name in ("img.jpg", "PHOTO.JPEG", "raster.PNG", "shot.heic"):
        assert classify(name) is FileCategory.PHOTO


def test_audio_extensions() -> None:
    for name in ("song.mp3", "track.FLAC", "voice.opus"):
        assert classify(name) is FileCategory.AUDIO


def test_archive_and_installer() -> None:
    assert classify("backup.tar.gz") is FileCategory.ARCHIVE
    assert classify("setup.exe") is FileCategory.INSTALLER
    assert classify("app.deb") is FileCategory.INSTALLER


def test_no_extension_is_other() -> None:
    assert classify("README") is FileCategory.OTHER
    assert classify(".bashrc") is FileCategory.OTHER
    assert classify("trailing.") is FileCategory.OTHER


def test_unknown_extension() -> None:
    assert classify("data.weirdext") is FileCategory.OTHER
