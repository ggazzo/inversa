import Router from 'preact-router';
import { useEffect } from 'preact/hooks';
import { Navbar } from './components/Navbar';
import { ConnectionBar } from './components/ConnectionBar';
import { Dashboard } from './pages/Dashboard';
import { Control } from './pages/Control';
import { Recipes } from './pages/Recipes';
import { Boil } from './pages/Boil';
import { Settings } from './pages/Settings';
import { ConnectionManager } from './services/ConnectionManager';
import { activeTab, toastMessage, toastType } from './stores/state';

export function App() {
  useEffect(() => {
    ConnectionManager.init();
  }, []);

  function handleRouteChange(e) {
    activeTab.value = e.url;
  }

  return (
    <div class="min-h-screen bg-base-200 pb-16">
      <ConnectionBar />
      <main class="container mx-auto px-4 py-4 max-w-2xl">
        <Router onChange={handleRouteChange}>
          <Dashboard path="/" />
          <Control path="/control" />
          <Recipes path="/recipes" />
          <Boil path="/boil" />
          <Settings path="/settings" />
        </Router>
      </main>
      <Navbar />
      <Toast />
    </div>
  );
}

function Toast() {
  const msg = toastMessage.value;
  if (!msg) return null;

  const typeClass = {
    info: 'alert-info',
    success: 'alert-success',
    error: 'alert-error',
  }[toastType.value] || 'alert-info';

  return (
    <div class="toast toast-top toast-center z-[100]">
      <div class={`alert ${typeClass} py-2 px-4 text-sm shadow-lg`}>
        <span>{msg}</span>
      </div>
    </div>
  );
}
