#ifndef SplashScreenH
#define SplashScreenH

namespace SplashScreen
{
	ECORE_API void	Show		(LPCSTR imagePath, LPCSTR title = NULL);
	ECORE_API void	ShowFromMem	(const void* data, u32 size, LPCSTR title = NULL);
	ECORE_API void	SetStatus	(LPCSTR text);
	ECORE_API void	Hide		();
}

#endif
