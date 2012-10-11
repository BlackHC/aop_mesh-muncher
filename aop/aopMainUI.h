#include <memory>

namespace aop {
	struct Application;
	struct CandidateSidebarUI;

	std::shared_ptr<CandidateSidebarUI> createCandidateSidebarUI( Application *application );
}