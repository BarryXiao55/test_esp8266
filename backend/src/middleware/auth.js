const { validateToken, updateLastSeen } = require('../db/queries');

function sensorAuth(req, res, next) {
  const auth = req.headers.authorization;
  if (!auth || !auth.startsWith('Bearer ')) {
    return res.status(401).json({ error: '缺少认证 token' });
  }

  const token = auth.slice(7);
  const deviceId = req.headers['x-device-id'];
  if (!deviceId) {
    return res.status(400).json({ error: '缺少 X-Device-Id 头' });
  }

  if (!validateToken(deviceId, token)) {
    return res.status(403).json({ error: 'token 无效或设备未注册' });
  }

  updateLastSeen(deviceId);
  req.deviceId = deviceId;
  next();
}

module.exports = { sensorAuth };
