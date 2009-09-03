#include "soundmanager.h"

#include "ifilesystem.h"
#include "archivelib.h"
#include "generic/callback.h"

#include <iostream>

namespace sound
{
	// Constructor
	SoundManager::SoundManager ()
	{
		playbackEnabled = false;
		resumingFileNameToBePlayed = "";  //  "maledeath01.ogg";  for testing :)
	}

	// Destructor
	SoundManager::~SoundManager ()
	{
	}

	bool SoundManager::playSound (const std::string& fileName)
	{
		if(!playbackEnabled)
			return true;

		// Make a copy of the filename
		std::string name = fileName;

		// Try to open the file as it is
		AutoPtr<ArchiveFile> file(GlobalFileSystem().openFile(name.c_str()));
		std::cout << "Trying: " << name << std::endl;
		if (file) {
			// File found, play it
			std::cout << "Found file: " << name << std::endl;
			resumingFileNameToBePlayed = fileName;
			if(playbackEnabled)
				_soundPlayer.play(*file);
			return true;
		}

		std::string root = name;
		// File not found, try to strip the extension
		if (name.rfind(".") != std::string::npos) {
			root = name.substr(0, name.rfind("."));
		}

		// Try to open the .ogg variant
		name = root + ".ogg";
		std::cout << "Trying: " << name << std::endl;
		AutoPtr<ArchiveFile> oggFile(GlobalFileSystem().openFile(name.c_str()));
		if (oggFile) {
			std::cout << "Found file: " << name << std::endl;
			resumingFileNameToBePlayed = fileName;
			if(playbackEnabled)
				_soundPlayer.play(*oggFile);
			return true;
		}

		// Try to open the file with .wav extension
		name = root + ".wav";
		std::cout << "Trying: " << name << std::endl;
		AutoPtr<ArchiveFile> wavFile(GlobalFileSystem().openFile(name.c_str()));
		if (wavFile) {
			std::cout << "Found file: " << name << std::endl;
			resumingFileNameToBePlayed = fileName;
			if(playbackEnabled)
				_soundPlayer.play(*wavFile);
			return true;
		}

		// File not found
		return false;
	}

	void SoundManager::stopSound ()
	{
		_soundPlayer.stop();
		resumingFileNameToBePlayed = "";
	}

	void SoundManager::switchPlaybackEnabledFlag ()
	{
		playbackEnabled = !playbackEnabled;
		if(playbackEnabled && resumingFileNameToBePlayed.length())
			playSound (resumingFileNameToBePlayed);
	}

} // namespace sound

bool GlobalSoundManager_isPlaybackEnabled(void)
{
	sound::SoundManager * sm = dynamic_cast<sound::SoundManager *>(GlobalSoundManager());
	return sm->isPlaybackEnabled();
}

void GlobalSoundManager_switchPlaybackEnabledFlag()
{
	sound::SoundManager * sm = dynamic_cast<sound::SoundManager *>(GlobalSoundManager());
	sm->switchPlaybackEnabledFlag();
}

/*!  Toggle menu callback definitions  */
typedef FreeCaller1<const BoolImportCallback&, &BoolFunctionExport<GlobalSoundManager_isPlaybackEnabled>::apply> SoundPlaybackEnabledApplyCaller;
SoundPlaybackEnabledApplyCaller g_soundPlaybackEnabled_button_caller;
BoolExportCallback g_soundPlaybackEnabled_button_callback(g_soundPlaybackEnabled_button_caller);

ToggleItem g_soundPlaybackEnabled_button(g_soundPlaybackEnabled_button_callback);
